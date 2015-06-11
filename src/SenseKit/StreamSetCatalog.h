#ifndef STREAMSETCATALOG_H
#define STREAMSETCATALOG_H

#include <map>
#include "StreamSet.h"
#include "StreamRegisteredEventArgs.h"
#include "StreamUnregisteringEventArgs.h"
#include <string>
#include <vector>
#include <memory>
#include "Logger.h"

namespace sensekit {

    class StreamSetCatalog
    {
    public:
        StreamSetCatalog()
            : m_logger("StreamSetCatalog")
        {}

        ~StreamSetCatalog();

        StreamSetConnection& open_set_connection(std::string uri);
        void close_set_connection(StreamSetConnection* connection);
        StreamSet& get_or_add(std::string uri, bool claim = false);
        StreamSet* find_streamset_for_stream(Stream* stream);

        void visit_sets(std::function<void(StreamSet*)> visitorMethod);
        void destroy_set(StreamSet* set);

        sensekit_callback_id_t register_for_stream_registered_event(StreamRegisteredCallback callback)
        {
            sensekit_callback_id_t id =  m_streamRegisteredSignal += callback;

            visit_sets(
                [&callback] (StreamSet* set)
                {
                    set->visit_streams(
                        [&callback, &set] (Stream* stream)
                        {
                            callback(StreamRegisteredEventArgs(set, stream, stream->get_description()));
                        });
                });

            return id;
        }

        sensekit_callback_id_t register_for_stream_unregistering_event(StreamUnregisteringCallback callback)
        {
            return m_streamUnregisteringSignal += callback;
        }

        void unregister_for_stream_registered_event(sensekit_callback_id_t callbackId)
        {
            m_streamRegisteredSignal -= callbackId;
        }

        void unregister_form_stream_unregistering_event(sensekit_callback_id_t callbackId)
        {
            m_streamUnregisteringSignal -= callbackId;
        }

    private:
        using StreamSetPtr = std::unique_ptr<StreamSet>;

        struct StreamSetEntry
        {
            StreamSetPtr streamSet;
            sensekit_callback_id_t addingId;
            sensekit_callback_id_t removingId;

            StreamSetEntry(StreamSetPtr setPtr, sensekit_callback_id_t addingId, sensekit_callback_id_t removingId)
                : streamSet(std::move(setPtr)), addingId(addingId), removingId(removingId)
            { }

            ~StreamSetEntry()
            {
                streamSet->unregister_for_stream_registered_event(addingId);
                streamSet->unregister_for_stream_unregistering_event(removingId);
            }
        };

        using StreamSetEntryPtr = std::unique_ptr<StreamSetEntry>;
        using StreamSetMap = std::map<std::string, StreamSetEntryPtr>;

        StreamSetMap m_streamSets;
        Logger m_logger;

        void on_stream_registered(StreamRegisteredEventArgs args);
        void on_stream_unregistering(StreamUnregisteringEventArgs args);

        Signal<StreamRegisteredEventArgs> m_streamRegisteredSignal;
        Signal<StreamUnregisteringEventArgs> m_streamUnregisteringSignal;
    };
}

#endif /* STREAMSETCATALOG_H */
