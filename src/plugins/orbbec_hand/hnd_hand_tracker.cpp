// This file is part of the Orbbec Astra SDK [https://orbbec3d.com]
// Copyright (c) 2015 Orbbec 3D
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Be excellent to each other.
// Undeprecate CRT functions
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "hnd_hand_tracker.hpp"
#include "hnd_segmentation.hpp"
#include <astra/capi/streams/hand_types.h>
#include <astra/capi/astra_ctypes.h>
#include <astra_core/plugins/astra_plugin.hpp>
#include <Shiny.h>

namespace astra { namespace hand {

    using namespace std;

    hand_tracker::hand_tracker(pluginservice_proxy& pluginService,
                               astra_streamset_t streamSet,
                               stream_description& depthDesc,
                               hand_settings& settings) :
        streamset_(plugins::get_uri_for_streamset(pluginService, streamSet)),
        reader_(streamset_.create_reader()),
        depthStream_(reader_.stream<depthstream>(depthDesc.subtype())),
        settings_(settings),
        pluginService_(pluginService),
        depthUtility_(settings.processingSizeWidth, settings.processingSizeHeight, settings.depthUtilitySettings),
        pointProcessor_(settings.pointProcessorSettings),
        processingSizeWidth_(settings.processingSizeWidth),
        processingSizeHeight_(settings.processingSizeHeight)

    {
        PROFILE_FUNC();

        create_streams(pluginService_, streamSet);
        depthStream_.start();

        reader_.stream<pointstream>().start();
        reader_.add_listener(*this);
    }

    hand_tracker::~hand_tracker()
    {
        PROFILE_FUNC();
        if (worldPoints_ != nullptr)
        {
            delete[] worldPoints_;
            worldPoints_ = nullptr;
        }
    }

    void hand_tracker::create_streams(pluginservice_proxy& pluginService, astra_streamset_t streamSet)
    {
        PROFILE_FUNC();
        LOG_INFO("hand_tracker", "creating hand streams");
        auto hs = plugins::make_stream<handstream>(pluginService, streamSet, ASTRA_HANDS_MAX_HAND_COUNT);
        handStream_ = std::unique_ptr<handstream>(std::move(hs));

        const int bytesPerPixel = 3;
        auto dhs = plugins::make_stream<debug_handstream>(pluginService,
                                                          streamSet,
                                                          processingSizeWidth_,
                                                          processingSizeHeight_,
                                                          bytesPerPixel);
        debugimagestream_ = std::unique_ptr<debug_handstream>(std::move(dhs));
    }

    void hand_tracker::on_frame_ready(stream_reader& reader, frame& frame)
    {
        PROFILE_FUNC();
        if (handStream_->has_connections() ||
            debugimagestream_->has_connections())
        {
            depthframe depthFrame = frame.get<depthframe>();
            pointframe pointFrame = frame.get<pointframe>();
            update_tracking(depthFrame, pointFrame);
        }

        PROFILE_UPDATE();
    }

    void hand_tracker::reset()
    {
        PROFILE_FUNC();
        depthUtility_.reset();
        pointProcessor_.reset();
    }

    void hand_tracker::update_tracking(depthframe& depthFrame, pointframe& pointFrame)
    {
        PROFILE_FUNC();
        if (!debugimagestream_->pause_input())
        {
            depthUtility_.depth_to_velocity_signal(depthFrame, matDepth_, matDepthFullSize_, matVelocitySignal_);
        }

        track_points(matDepth_, matDepthFullSize_, matVelocitySignal_, pointFrame.data());

        //use same frameIndex as source depth frame
        astra_frame_index_t frameIndex = depthFrame.frameIndex();

        if (handStream_->has_connections())
        {
            generate_hand_frame(frameIndex);
        }

        if (debugimagestream_->has_connections())
        {
            generate_hand_debug_image_frame(frameIndex);
        }
    }

    void hand_tracker::track_points(cv::Mat& matDepth,
                                    cv::Mat& matDepthFullSize,
                                    cv::Mat& matVelocitySignal,
                                    const vector3f* fullSizeWorldPoints)
    {
        PROFILE_FUNC();

        layerSegmentation_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);
        layerScore_ = cv::Mat::zeros(matDepth.size(), CV_32FC1);
        layerEdgeDistance_ = cv::Mat::zeros(matDepth.size(), CV_32FC1);
        debugUpdateSegmentation_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);
        debugCreateSegmentation_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);
        debugRefineSegmentation_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);
        updateForegroundSearched_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);
        createForegroundSearched_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);
        refineForegroundSearched_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);
        debugUpdateScore_ = cv::Mat::zeros(matDepth.size(), CV_32FC1);
        debugCreateScore_ = cv::Mat::zeros(matDepth.size(), CV_32FC1);
        matDepthWindow_ = cv::Mat::zeros(matDepth.size(), CV_32FC1);
        refineSegmentation_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);
        refineScore_ = cv::Mat::zeros(matDepth.size(), CV_32FC1);
        refineEdgeDistance_ = cv::Mat::zeros(matDepth.size(), CV_32FC1);
        debugUpdateScoreValue_ = cv::Mat::zeros(matDepth.size(), CV_32FC1);
        debugCreateScoreValue_ = cv::Mat::zeros(matDepth.size(), CV_32FC1);
        debugRefineScoreValue_ = cv::Mat::zeros(matDepth.size(), CV_32FC1);
        debugCreateTestPassMap_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);
        debugUpdateTestPassMap_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);
        debugRefineTestPassMap_ = cv::Mat::zeros(matDepth.size(), CV_8UC1);

        int numPoints = matDepth.cols * matDepth.rows;
        if (worldPoints_ == nullptr || numWorldPoints_ != numPoints)
        {
            if (worldPoints_ != nullptr)
            {
                delete[] worldPoints_;
                worldPoints_ = nullptr;
            }

            numWorldPoints_ = numPoints;
            worldPoints_ = new astra::vector3f[numPoints];
        }

        const conversion_cache_t depthToWorldData = depthStream_.depth_to_world_data();

        bool debugLayersEnabled = debugimagestream_->has_connections();
        bool enabledTestPassMap = debugimagestream_->view_type() == DEBUG_HAND_VIEW_TEST_PASS_MAP;

        tracking_matrices updateMatrices(matDepthFullSize,
                                         matDepth,
                                         matArea_,
                                         matAreaSqrt_,
                                         matVelocitySignal,
                                         updateForegroundSearched_,
                                         layerSegmentation_,
                                         layerScore_,
                                         layerEdgeDistance_,
                                         layerIntegralArea_,
                                         layerTestPassMap_,
                                         debugUpdateSegmentation_,
                                         debugUpdateScore_,
                                         debugUpdateScoreValue_,
                                         debugUpdateTestPassMap_,
                                         enabledTestPassMap,
                                         fullSizeWorldPoints,
                                         worldPoints_,
                                         debugLayersEnabled,
                                         depthStream_.coordinateMapper(),
                                         depthToWorldData);

        if (!debugimagestream_->pause_input())
        {
            pointProcessor_.initialize_common_calculations(updateMatrices);
        }

        //Update existing points first so that if we lose a point, we might recover it in the "add new" stage below
        //without having at least one frame of a lost point.

        pointProcessor_.update_tracked_points(updateMatrices);

        pointProcessor_.remove_duplicate_points();

        tracking_matrices createMatrices(matDepthFullSize,
                                         matDepth,
                                         matArea_,
                                         matAreaSqrt_,
                                         matVelocitySignal,
                                         createForegroundSearched_,
                                         layerSegmentation_,
                                         layerScore_,
                                         layerEdgeDistance_,
                                         layerIntegralArea_,
                                         layerTestPassMap_,
                                         debugCreateSegmentation_,
                                         debugCreateScore_,
                                         debugCreateScoreValue_,
                                         debugCreateTestPassMap_,
                                         enabledTestPassMap,
                                         fullSizeWorldPoints,
                                         worldPoints_,
                                         debugLayersEnabled,
                                         depthStream_.coordinateMapper(),
                                         depthToWorldData);

        //add new points (unless already tracking)
        if (!debugimagestream_->use_mouse_probe())
        {
            cv::Point seedPosition;
            cv::Point nextSearchStart(0, 0);
            while (segmentation::find_next_velocity_seed_pixel(matVelocitySignal, createForegroundSearched_, seedPosition, nextSearchStart))
            {
                pointProcessor_.update_tracked_or_create_new_point_from_seed(createMatrices, seedPosition);
            }
        }
        else
        {
            debug_spawn_point(createMatrices);
        }

        debug_probe_point(createMatrices);

        //remove old points
        pointProcessor_.remove_stale_or_dead_points();

        tracking_matrices refinementMatrices(matDepthFullSize,
                                             matDepthWindow_,
                                             matArea_,
                                             matAreaSqrt_,
                                             matVelocitySignal,
                                             refineForegroundSearched_,
                                             refineSegmentation_,
                                             refineScore_,
                                             refineEdgeDistance_,
                                             layerIntegralArea_,
                                             layerTestPassMap_,
                                             debugRefineSegmentation_,
                                             debugRefineScore_,
                                             debugRefineScoreValue_,
                                             debugRefineTestPassMap_,
                                             enabledTestPassMap,
                                             fullSizeWorldPoints,
                                             worldPoints_,
                                             false,
                                             depthStream_.coordinateMapper(),
                                             depthToWorldData);

        pointProcessor_.update_full_resolution_points(refinementMatrices);

        pointProcessor_.update_trajectories();
    }

    void hand_tracker::debug_probe_point(tracking_matrices& matrices)
    {
        if (!debugimagestream_->use_mouse_probe())
        {
            return;
        }

        cv::Point probePosition = get_mouse_probe_position();

        cv::Mat& matDepth = matrices.depth;

        float depth = matDepth.at<float>(probePosition);
        float score = debugCreateScoreValue_.at<float>(probePosition);
        float edgeDist = layerEdgeDistance_.at<float>(probePosition);

        auto segmentationSettings = settings_.pointProcessorSettings.segmentationSettings;

        const test_behavior outputTestLog = TEST_BEHAVIOR_LOG;
        const test_phase phase = TEST_PHASE_CREATE;

        bool validPointInRange = segmentation::test_point_in_range(matrices,
                                                                   probePosition,
                                                                   outputTestLog);
        bool validPointArea = false;
        bool validRadiusTest = false;
        bool validNaturalEdges = false;

        if (validPointInRange)
        {
            validPointArea = segmentation::test_point_area_integral(matrices,
                                                                    matrices.layerIntegralArea,
                                                                    segmentationSettings.areaTestSettings,
                                                                    probePosition,
                                                                    phase,
                                                                    outputTestLog);
            validRadiusTest = segmentation::test_foreground_radius_percentage(matrices,
                                                                              segmentationSettings.circumferenceTestSettings,
                                                                              probePosition,
                                                                              phase,
                                                                              outputTestLog);

            validNaturalEdges = segmentation::test_natural_edges(matrices,
                                                                 segmentationSettings.naturalEdgeTestSettings,
                                                                 probePosition,
                                                                 phase,
                                                                 outputTestLog);
        }

        bool allPointsPass = validPointInRange &&
            validPointArea &&
            validRadiusTest &&
            validNaturalEdges;

        LOG_INFO("hand_tracker", "depth: %f score: %f edge %f tests: %s",
                 depth,
                 score,
                 edgeDist,
                 allPointsPass ? "PASS" : "FAIL");
    }

    void hand_tracker::debug_spawn_point(tracking_matrices& matrices)
    {
        if (!debugimagestream_->pause_input())
        {
            pointProcessor_.initialize_common_calculations(matrices);
        }
        cv::Point seedPosition = get_spawn_position();

        pointProcessor_.update_tracked_or_create_new_point_from_seed(matrices, seedPosition);
    }

    cv::Point hand_tracker::get_spawn_position()
    {
        auto normPosition = debugimagestream_->mouse_norm_position();

        if (debugimagestream_->spawn_point_locked())
        {
            normPosition = debugimagestream_->spawn_norm_position();
        }

        int x = MAX(0, MIN(processingSizeWidth_, normPosition.x * processingSizeWidth_));
        int y = MAX(0, MIN(processingSizeHeight_, normPosition.y * processingSizeHeight_));
        return cv::Point(x, y);
    }

    cv::Point hand_tracker::get_mouse_probe_position()
    {
        auto normPosition = debugimagestream_->mouse_norm_position();
        int x = MAX(0, MIN(processingSizeWidth_, normPosition.x * processingSizeWidth_));
        int y = MAX(0, MIN(processingSizeHeight_, normPosition.y * processingSizeHeight_));
        return cv::Point(x, y);
    }

    void hand_tracker::generate_hand_frame(astra_frame_index_t frameIndex)
    {
        PROFILE_FUNC();

        astra_handframe_wrapper_t* handFrame = handStream_->begin_write(frameIndex);

        if (handFrame != nullptr)
        {
            handFrame->frame.handpoints = reinterpret_cast<astra_handpoint_t*>(&(handFrame->frame_data));
            handFrame->frame.handCount = ASTRA_HANDS_MAX_HAND_COUNT;

            update_hand_frame(pointProcessor_.get_trackedPoints(), handFrame->frame);

            PROFILE_BEGIN(end_write);
            handStream_->end_write();
            PROFILE_END();
        }
    }

    void hand_tracker::generate_hand_debug_image_frame(astra_frame_index_t frameIndex)
    {
        PROFILE_FUNC();
        astra_imageframe_wrapper_t* debugimageframe = debugimagestream_->begin_write(frameIndex);

        if (debugimageframe != nullptr)
        {
            debugimageframe->frame.data = reinterpret_cast<uint8_t *>(&(debugimageframe->frame_data));

            astra_image_metadata_t metadata;

            metadata.width = processingSizeWidth_;
            metadata.height = processingSizeHeight_;
            metadata.pixelFormat = astra_pixel_formats::ASTRA_PIXEL_FORMAT_RGB888;

            debugimageframe->frame.metadata = metadata;
            update_debug_image_frame(debugimageframe->frame);

            debugimagestream_->end_write();
        }
    }

    void hand_tracker::update_hand_frame(vector<tracked_point>& internaltracked_points, _astra_handframe& frame)
    {
        PROFILE_FUNC();
        int handIndex = 0;
        int maxHandCount = frame.handCount;

        bool includeCandidates = handStream_->include_candidate_points();

        for (auto it = internaltracked_points.begin(); it != internaltracked_points.end(); ++it)
        {
            tracked_point internalPoint = *it;

            tracking_status status = internalPoint.trackingStatus;
            tracked_point_type pointType = internalPoint.pointType;

            bool includeByStatus = status == tracking_status::tracking ||
                status == tracking_status::lost;
            bool includeByType = pointType == tracked_point_type::active_point ||
                (pointType == tracked_point_type::candidate_point && includeCandidates);
            if (includeByStatus && includeByType && handIndex < maxHandCount)
            {
                astra_handpoint_t& point = frame.handpoints[handIndex];
                ++handIndex;

                point.trackingId = internalPoint.trackingId;

                point.depthPosition.x = internalPoint.fullSizePosition.x;
                point.depthPosition.y = internalPoint.fullSizePosition.y;

                copy_position(internalPoint.fullSizeWorldPosition, point.worldPosition);
                copy_position(internalPoint.fullSizeWorldDeltaPosition, point.worldDeltaPosition);

                point.status = convert_hand_status(status, pointType);
            }
        }
        for (int i = handIndex; i < maxHandCount; ++i)
        {
            astra_handpoint_t& point = frame.handpoints[i];
            reset_hand_point(point);
        }
    }

    void hand_tracker::copy_position(cv::Point3f& source, astra_vector3f_t& target)
    {
        PROFILE_FUNC();
        target.x = source.x;
        target.y = source.y;
        target.z = source.z;
    }

    astra_handstatus_t hand_tracker::convert_hand_status(tracking_status status, tracked_point_type type)
    {
        PROFILE_FUNC();
        if (type == tracked_point_type::candidate_point)
        {
            return HAND_STATUS_CANDIDATE;
        }
        switch (status)
        {
        case tracking_status::tracking:
            return HAND_STATUS_TRACKING;
            break;
        case tracking_status::lost:
            return HAND_STATUS_LOST;
            break;
        case tracking_status::dead:
        case tracking_status::not_tracking:
        default:
            return HAND_STATUS_NOTTRACKING;
            break;
        }
    }

    void hand_tracker::reset_hand_point(astra_handpoint_t& point)
    {
        PROFILE_FUNC();
        point.trackingId = -1;
        point.status = HAND_STATUS_NOTTRACKING;
        point.depthPosition = astra_vector2i_t();
        point.worldPosition = astra_vector3f_t();
        point.worldDeltaPosition = astra_vector3f_t();
    }

    void mark_image_pixel(_astra_imageframe& imageFrame,
                          rgb_pixel color,
                          astra::vector2i p)
    {
        PROFILE_FUNC();
        rgb_pixel* colorData = static_cast<rgb_pixel*>(imageFrame.data);
        int index = p.x + p.y * imageFrame.metadata.width;
        colorData[index] = color;
    }

    void hand_tracker::overlay_circle(_astra_imageframe& imageFrame)
    {
        PROFILE_FUNC();

        float resizeFactor = matDepthFullSize_.cols / static_cast<float>(matDepth_.cols);
        scaling_coordinate_mapper mapper(depthStream_.depth_to_world_data(), resizeFactor);

        rgb_pixel color(255, 0, 255);

        auto segmentationSettings = settings_.pointProcessorSettings.segmentationSettings;
        float foregroundRadius1 = segmentationSettings.circumferenceTestSettings.foregroundRadius1;
        float foregroundRadius2 = segmentationSettings.circumferenceTestSettings.foregroundRadius2;

        cv::Point probePosition = get_mouse_probe_position();

        std::vector<astra::vector2i> points;

        segmentation::get_circumference_points(matDepth_, probePosition, foregroundRadius1, mapper, points);

        for (auto p : points)
        {
            mark_image_pixel(imageFrame, color, p);
        }

        segmentation::get_circumference_points(matDepth_, probePosition, foregroundRadius2, mapper, points);

        for (auto p : points)
        {
            mark_image_pixel(imageFrame, color, p);
        }

        cv::Point spawnPosition = get_spawn_position();
        rgb_pixel spawnColor(255, 0, 255);

        mark_image_pixel(imageFrame, spawnColor, vector2i(spawnPosition.x, spawnPosition.y));
    }

    void hand_tracker::update_debug_image_frame(_astra_imageframe& colorFrame)
    {
        PROFILE_FUNC();
        float maxVelocity_ = 0.1;

        rgb_pixel foregroundColor(0, 0, 255);
        rgb_pixel searchedColor(128, 255, 0);
        rgb_pixel searchedColor2(0, 128, 255);
        rgb_pixel testPassColor(0, 255, 128);

        debug_handview_type view = debugimagestream_->view_type();

        switch (view)
        {
        case DEBUG_HAND_VIEW_DEPTH:
            debugVisualizer_.show_depth_matrix(matDepth_,
                                               colorFrame);
            break;
        case DEBUG_HAND_VIEW_DEPTH_MOD:
            debugVisualizer_.show_depth_matrix(depthUtility_.matDepthFilled(),
                                               colorFrame);
            break;
        case DEBUG_HAND_VIEW_DEPTH_AVG:
            debugVisualizer_.show_depth_matrix(depthUtility_.matDepthAvg(),
                                               colorFrame);
            break;
        case DEBUG_HAND_VIEW_VELOCITY:
            debugVisualizer_.show_velocity_matrix(depthUtility_.matDepthVel(),
                                                  maxVelocity_,
                                                  colorFrame);
            break;
        case DEBUG_HAND_VIEW_FILTEREDVELOCITY:
            debugVisualizer_.show_velocity_matrix(depthUtility_.matDepthVelErode(),
                                                  maxVelocity_,
                                                  colorFrame);
            break;
        case DEBUG_HAND_VIEW_UPDATE_SEGMENTATION:
            debugVisualizer_.show_norm_array<char>(debugUpdateSegmentation_,
                                                   debugUpdateSegmentation_,
                                                   colorFrame);
            break;
        case DEBUG_HAND_VIEW_CREATE_SEGMENTATION:
            debugVisualizer_.show_norm_array<char>(debugCreateSegmentation_,
                                                   debugCreateSegmentation_,
                                                   colorFrame);
            break;
        case DEBUG_HAND_VIEW_UPDATE_SEARCHED:
        case DEBUG_HAND_VIEW_CREATE_SEARCHED:
            debugVisualizer_.show_depth_matrix(matDepth_,
                                               colorFrame);
            break;
        case DEBUG_HAND_VIEW_CREATE_SCORE:
            debugVisualizer_.show_norm_array<float>(debugCreateScore_,
                                                    debugCreateSegmentation_,
                                                    colorFrame);
            break;
        case DEBUG_HAND_VIEW_UPDATE_SCORE:
            debugVisualizer_.show_norm_array<float>(debugUpdateScore_,
                                                    debugUpdateSegmentation_,
                                                    colorFrame);
            break;
        case DEBUG_HAND_VIEW_HANDWINDOW:
            debugVisualizer_.show_depth_matrix(matDepthWindow_,
                                               colorFrame);
            break;
        case DEBUG_HAND_VIEW_TEST_PASS_MAP:
            debugVisualizer_.show_norm_array<char>(debugCreateTestPassMap_,
                                                   debugCreateTestPassMap_,
                                                   colorFrame);
            break;
        }

        if (view != DEBUG_HAND_VIEW_HANDWINDOW &&
            view != DEBUG_HAND_VIEW_CREATE_SCORE &&
            view != DEBUG_HAND_VIEW_UPDATE_SCORE &&
            view != DEBUG_HAND_VIEW_DEPTH_MOD &&
            view != DEBUG_HAND_VIEW_DEPTH_AVG &&
            view != DEBUG_HAND_VIEW_TEST_PASS_MAP)
        {
            if (view == DEBUG_HAND_VIEW_CREATE_SEARCHED)
            {
                debugVisualizer_.overlay_mask(createForegroundSearched_, colorFrame, searchedColor, pixel_type::searched);
                debugVisualizer_.overlay_mask(createForegroundSearched_, colorFrame, searchedColor2, pixel_type::searched_from_out_of_range);
            }
            else if (view == DEBUG_HAND_VIEW_UPDATE_SEARCHED)
            {
                debugVisualizer_.overlay_mask(updateForegroundSearched_, colorFrame, searchedColor, pixel_type::searched);
                debugVisualizer_.overlay_mask(updateForegroundSearched_, colorFrame, searchedColor2, pixel_type::searched_from_out_of_range);
            }

            debugVisualizer_.overlay_mask(matVelocitySignal_, colorFrame, foregroundColor, pixel_type::foreground);
        }

        if (debugimagestream_->use_mouse_probe())
        {
            overlay_circle(colorFrame);
        }
        debugVisualizer_.overlay_crosshairs(pointProcessor_.get_trackedPoints(), colorFrame);
    }
}}
