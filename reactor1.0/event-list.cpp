#include "event_list.h"

void EventList::ExtractAndHandle()
{
    if (!events.empty() && pthread_spin_trylock(&lock) == 0)
    {
        auto event = events.front();
        event.HandleEvent();
        events.pop();
        pthread_spin_unlock(&lock);
    }
}

void EventList::AddEvent(Event &event)
{
    pthread_spin_lock(&lock);
    //LOG(INFO) << "add new event";
    events.push(event);
    pthread_spin_unlock(&lock);
}