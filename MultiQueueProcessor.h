#pragma once
#include <atomic>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <pthread.h>
#include <random>
#include <set>
#include <thread>
#include "QString"
#include <iostream>


template<typename Key, typename Value>
struct IConsumer
{
    virtual void Consume(Key id, const Value &value)
    {        
        (void)id;
        (void)value;
       // std::cerr << "Consume" << id  << value << std::endl;
    }    
    virtual ~IConsumer() = default;
};

template<typename Key, typename Value>
class MultiQueueProcessor
{
public:
    MultiQueueProcessor() :
        running{ true },
        th(std::bind(&MultiQueueProcessor::Process, this)) {}

    MultiQueueProcessor(const MultiQueueProcessor&  o) =  delete;
    MultiQueueProcessor(      MultiQueueProcessor&& o) =  delete;
    MultiQueueProcessor& operator=(const MultiQueueProcessor&  o) =  delete;
    MultiQueueProcessor& operator=(      MultiQueueProcessor&& o) =  delete;
//----------------------------------------------------------------------------
    virtual ~MultiQueueProcessor()
    {
        StopProcessing();
        th.join();
    }
//----------------------------------------------------------------------------
    // For prod release i should
    // adding posibility to relaunch "Procces()" without recreating,but , i think, it's not for test task.
    void StopProcessing (           ) {       running      = false;}
//----------------------------------------------------------------------------
    void SetMaxCapacity (size_t cap ) {       maxCapacity  = cap  ;}
//----------------------------------------------------------------------------
    void SetMaxThreads ( uint cnt   ) {       maxThreads   = cnt  ;} //for example cnt = std::thread::hardware_concurrency();
//----------------------------------------------------------------------------
    bool isRunning      (          ) {return running             ;}
//----------------------------------------------------------------------------
    virtual bool Subscribe(const Key& id, IConsumer<Key, Value> * consumer)
    {
        bool rv = false;
        if(isRunning() && consumer)
        {
            std::scoped_lock<std::recursive_mutex,std::recursive_mutex> lock{qmtx, cmtx };
            auto it = queHndl.find(id);
            if(it != queHndl.end())
            {
                if(consumers.find(id) == consumers.end())
                {
                    consumers.emplace(id,consumer);
                    rv =  true;
                }
            }
        }
        return rv;
    }
//----------------------------------------------------------------------------
    virtual bool Unsubscribe(const Key& id)
    {
        //std::scoped_lock<std::recursive_mutex,std::recursive_mutex> lock{qmtx, cmtx };
        std::scoped_lock<std::recursive_mutex> lock{cmtx };
        return consumers.erase(id);
    }
//----------------------------------------------------------------------------
    virtual bool Enqueue(const Key& id, Value value)
    {        
        //std::scoped_lock<std::recursive_mutex,std::recursive_mutex> lock{qmtx, cmtx };
        std::scoped_lock<std::recursive_mutex> lock{qmtx};
        //one entry point -  one exit point
        bool rv = false;
        bool contains = queues.find(id) != queues.end();
        if(contains || (queues.size() < maxThreads))
        {
            auto& iter = queues[id];
            if (iter.size() < maxCapacity)
            {
//                std::cerr <<  "Enqueue!" << id << value << std::endl ;
                iter.push_back(value);
                rv = true;
            }
        }
        return rv;
    }
//----------------------------------------------------------------------------
    virtual Value Dequeue(const Key& id,bool* isOk = nullptr)
    {
        //std::scoped_lock<std::recursive_mutex,std::recursive_mutex> lock{qmtx, cmtx };

        std::scoped_lock<std::recursive_mutex> lock{qmtx};
        auto iter = queues.find(id);
        if (iter != queues.end())
        {
            auto& list = iter->second;
            if (list.size())
            {
                auto front = list.front();
                list.pop_front();
                //std::cerr <<  "Dequeue!"  << std::endl;
                if(isOk) *isOk = true;
                return front;
            }
        }
        return Value{};
    }
//----------------------------------------------------------------------------
protected:
    virtual void Process()
    {
        while (running)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::scoped_lock<std::recursive_mutex,std::recursive_mutex>(qmtx,cmtx);
            for (auto&& [first,second] : queues)
            {
                auto& key = first;
                queHndl.try_emplace(key,addThread,this,key);
            }
        }
        for(auto& it :queHndl ) it.second.join();
        std::cerr  << "Process DONE!" << std::endl;
    }


    std::unordered_map<Key, IConsumer<Key, Value> *> consumers;
    std::unordered_map<Key, std::list<Value>>        queues   ;

    std::recursive_mutex      qmtx   ;
    std::recursive_mutex      cmtx   ;    
private:
    //----------------------------------------------------------------------------
    static void addThread(MultiQueueProcessor* mqp ,const Key& id)
    {
        while(mqp && mqp->isRunning())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::scoped_lock<std::recursive_mutex,std::recursive_mutex>(mqp->qmtx,mqp->cmtx);
            auto consumerIter = mqp->consumers.find(id);
            if(consumerIter != mqp->consumers.end())
            {
                bool isOk =  false;
                Value front = mqp->Dequeue(id,&isOk);
                if (isOk) consumerIter->second->Consume(id, front);
            }
        }
    }
    std::atomic_bool          running           ;
    std::thread               th                ;
    std::map<Key,std::thread> queHndl           ;
    size_t                    maxCapacity = 1000;
    size_t                    maxThreads  = 10  ;
};

