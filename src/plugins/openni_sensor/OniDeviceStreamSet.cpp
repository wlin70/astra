#include "OniDeviceStreamSet.h"
#include <Shiny.h>

namespace astra { namespace plugins {

    OniDeviceStreamSet::OniDeviceStreamSet(std::string name,
                                           PluginServiceProxy& pluginService,
                                           const char* uri)
        : m_pluginService(pluginService),
          m_uri(uri)
    {
        PROFILE_FUNC();
        m_uri = uri;
        m_pluginService.create_stream_set(name.c_str(), m_streamSetHandle);
    }

    OniDeviceStreamSet::~OniDeviceStreamSet()
    {
        PROFILE_FUNC();
        close();
        m_pluginService.destroy_stream_set(m_streamSetHandle);
    }

    astra_status_t OniDeviceStreamSet::open()
    {
        PROFILE_FUNC();
        if (m_isOpen)
            return ASTRA_STATUS_SUCCESS;

        SINFO("OniDeviceStreamSet", "opening device: %s", m_uri.c_str());
        openni::Status rc =  m_oniDevice.open(m_uri.c_str());

        if (rc != openni::STATUS_OK)
        {
            SWARN("OniDeviceStreamSet", "failed to open device: %s", openni::OpenNI::getExtendedError());
            return ASTRA_STATUS_DEVICE_ERROR;
        }

        SINFO("OniDeviceStreamSet", "opened device: %s", m_uri.c_str());

        open_sensor_streams();

        m_isOpen = true;

        return ASTRA_STATUS_SUCCESS;
    }

    astra_status_t OniDeviceStreamSet::close()
    {
        PROFILE_FUNC();
        if (!m_isOpen)
            return ASTRA_STATUS_SUCCESS;

        close_sensor_streams();

        SINFO("OniDeviceStreamSet", "closing oni device: %s", m_uri.c_str());
        m_oniDevice.close();

        m_isOpen = false;

        return ASTRA_STATUS_SUCCESS;
    }

    astra_status_t OniDeviceStreamSet::read()
    {
        PROFILE_BLOCK(streamset_read);
        if (!m_isOpen || m_streams.size() == 0)
            return ASTRA_STATUS_SUCCESS;

        int streamIndex = -1;
        int timeout = openni::TIMEOUT_NONE;

        openni::Status rc;
        int i = 0;

        do
        {
            rc = openni::OpenNI::waitForAnyStream(m_oniStreams.data(),
                                                  m_streams.size(),
                                                  &streamIndex,
                                                  timeout);

            if (streamIndex != -1) m_streams[streamIndex]->read_frame();
        } while (i++ < m_streams.size() && rc == openni::STATUS_OK);

        if (rc == openni::STATUS_TIME_OUT)
        {
            return ASTRA_STATUS_TIMEOUT;
        }

        return ASTRA_STATUS_SUCCESS;
    }

    astra_status_t OniDeviceStreamSet::open_sensor_streams()
    {
        PROFILE_FUNC();

        bool enableColor = true;
        if (enableColor && m_oniDevice.hasSensor(openni::SENSOR_COLOR))
        {
            OniColorStream* stream = new OniColorStream(m_pluginService,
                                                        m_streamSetHandle,
                                                        m_oniDevice);

            astra_status_t rc = ASTRA_STATUS_SUCCESS;
            rc = stream->open();

            if (rc == ASTRA_STATUS_SUCCESS)
            {
                rc = stream->start();
                if (rc == ASTRA_STATUS_SUCCESS)
                {
                    m_streams.push_back (StreamPtr(stream));
                    m_oniStreams [m_streams.size() - 1] = stream->get_oni_stream();
                }
            }

            if ( rc != ASTRA_STATUS_SUCCESS)
                SWARN("OniDeviceStreamSet", "unable to open openni color stream.");
        }

        if (m_oniDevice.hasSensor(openni::SENSOR_DEPTH))
        {
            OniDepthStream* stream = new OniDepthStream(m_pluginService,
                                                        m_streamSetHandle,
                                                        m_oniDevice);

            astra_status_t rc = ASTRA_STATUS_SUCCESS;
            rc = stream->open();

            if (rc == ASTRA_STATUS_SUCCESS)
            {
                rc = stream->start();
                if (rc == ASTRA_STATUS_SUCCESS)
                {
                    m_streams.push_back (StreamPtr(stream));
                    m_oniStreams [m_streams.size() - 1] = stream->get_oni_stream();
                }
            }

            if ( rc != ASTRA_STATUS_SUCCESS)
                SWARN("OniDeviceStreamSet", "unable to open openni depth stream.");
        }

        if (m_oniStreams.size() > 1)
        {
            m_oniDevice.setDepthColorSyncEnabled(true);
        }

        return ASTRA_STATUS_SUCCESS;
    }

    astra_status_t OniDeviceStreamSet::close_sensor_streams()
    {
        PROFILE_FUNC();
        m_streams.clear();
        m_oniStreams.fill(nullptr);

        return ASTRA_STATUS_SUCCESS;
    }
}}
