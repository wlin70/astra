/* THIS FILE AUTO-GENERATED FROM PluginServiceDelegate.h.lpp. DO NOT EDIT. */
#ifndef PLUGINSERVICEDELEGATE_H
#define PLUGINSERVICEDELEGATE_H

#include <SenseKit/sensekit_types.h>
#include <stdarg.h>
#include "SenseKitContext.h"

namespace sensekit {

    class PluginServiceDelegate
    {
    public:

        static sensekit_status_t register_stream_registered_callback(void* pluginService,
                                                                     stream_registered_callback_t callback,
                                                                     void* clientTag,
                                                                     sensekit_callback_id_t* callbackId)
        {
            return static_cast<PluginService*>(pluginService)->register_stream_registered_callback(callback, clientTag, *callbackId);
        }

        static sensekit_status_t register_stream_unregistering_callback(void* pluginService,
                                                                        stream_unregistering_callback_t callback,
                                                                        void* clientTag,
                                                                        sensekit_callback_id_t* callbackId)
        {
            return static_cast<PluginService*>(pluginService)->register_stream_unregistering_callback(callback, clientTag, *callbackId);
        }

        static sensekit_status_t register_host_event_callback(void* pluginService,
                                                              host_event_callback_t callback,
                                                              void* clientTag,
                                                              sensekit_callback_id_t* callbackId)
        {
            return static_cast<PluginService*>(pluginService)->register_host_event_callback(callback, clientTag, *callbackId);
        }

        static sensekit_status_t unregister_host_event_callback(void* pluginService,
                                                                sensekit_callback_id_t callback)
        {
            return static_cast<PluginService*>(pluginService)->unregister_host_event_callback(callback);
        }

        static sensekit_status_t unregister_stream_registered_callback(void* pluginService,
                                                                       sensekit_callback_id_t callback)
        {
            return static_cast<PluginService*>(pluginService)->unregister_stream_registered_callback(callback);
        }

        static sensekit_status_t unregister_stream_unregistering_callback(void* pluginService,
                                                                          sensekit_callback_id_t callback)
        {
            return static_cast<PluginService*>(pluginService)->unregister_stream_unregistering_callback(callback);
        }

        static sensekit_status_t create_stream_set(void* pluginService,
                                                   const char* setUri,
                                                   sensekit_streamset_t& setHandle)
        {
            return static_cast<PluginService*>(pluginService)->create_stream_set(setUri, setHandle);
        }

        static sensekit_status_t destroy_stream_set(void* pluginService,
                                                    sensekit_streamset_t& setHandle)
        {
            return static_cast<PluginService*>(pluginService)->destroy_stream_set(setHandle);
        }

        static sensekit_status_t get_streamset_uri(void* pluginService,
                                                   sensekit_streamset_t setHandle,
                                                   const char** uri)
        {
            return static_cast<PluginService*>(pluginService)->get_streamset_uri(setHandle, *uri);
        }

        static sensekit_status_t create_stream(void* pluginService,
                                               sensekit_streamset_t setHandle,
                                               sensekit_stream_desc_t desc,
                                               stream_callbacks_t pluginCallbacks,
                                               sensekit_stream_t* handle)
        {
            return static_cast<PluginService*>(pluginService)->create_stream(setHandle, desc, pluginCallbacks, *handle);
        }

        static sensekit_status_t destroy_stream(void* pluginService,
                                                sensekit_stream_t& handle)
        {
            return static_cast<PluginService*>(pluginService)->destroy_stream(handle);
        }

        static sensekit_status_t create_stream_bin(void* pluginService,
                                                   sensekit_stream_t streamHandle,
                                                   size_t lengthInBytes,
                                                   sensekit_bin_t* binHandle,
                                                   sensekit_frame_t** binBuffer)
        {
            return static_cast<PluginService*>(pluginService)->create_stream_bin(streamHandle, lengthInBytes, *binHandle, *binBuffer);
        }

        static sensekit_status_t destroy_stream_bin(void* pluginService,
                                                    sensekit_stream_t streamHandle,
                                                    sensekit_bin_t* binHandle,
                                                    sensekit_frame_t** binBuffer)
        {
            return static_cast<PluginService*>(pluginService)->destroy_stream_bin(streamHandle, *binHandle, *binBuffer);
        }

        static sensekit_status_t bin_has_connections(void* pluginService,
                                                     sensekit_bin_t binHandle,
                                                     bool* hasConnections)
        {
            return static_cast<PluginService*>(pluginService)->bin_has_connections(binHandle, *hasConnections);
        }

        static sensekit_status_t cycle_bin_buffers(void* pluginService,
                                                   sensekit_bin_t binHandle,
                                                   sensekit_frame_t** binBuffer)
        {
            return static_cast<PluginService*>(pluginService)->cycle_bin_buffers(binHandle, *binBuffer);
        }

        static sensekit_status_t link_connection_to_bin(void* pluginService,
                                                        sensekit_streamconnection_t connection,
                                                        sensekit_bin_t binHandle)
        {
            return static_cast<PluginService*>(pluginService)->link_connection_to_bin(connection, binHandle);
        }

        static sensekit_status_t get_parameter_bin(void* pluginService,
                                                   size_t byteSize,
                                                   sensekit_parameter_bin_t* binHandle,
                                                   sensekit_parameter_data_t* parameterData)
        {
            return static_cast<PluginService*>(pluginService)->get_parameter_bin(byteSize, *binHandle, *parameterData);
        }

        static sensekit_status_t log(void* pluginService,
                                     const char* channel,
                                     sensekit_log_severity_t logLevel,
                                     const char* format,
                                     va_list args)
        {
            return static_cast<PluginService*>(pluginService)->log(channel, logLevel, format, args);
        }
    };
}

#endif /* PLUGINSERVICEDELEGATE_H */
