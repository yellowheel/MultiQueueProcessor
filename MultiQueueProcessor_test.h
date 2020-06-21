#pragma once
#include "MultiQueueProcessor.h"

template<typename Key, typename Value>
class MQPTest:public MultiQueueProcessor<Key,Value>
{
public:
    MQPTest():MultiQueueProcessor<Key,Value>()
    {
         begin = std::chrono::steady_clock::now();
    }
    ~MQPTest() = default;
    Value Dequeue    (const Key& id,bool* isOk = nullptr             )
    {
        Value val = MultiQueueProcessor<Key,Value>::Dequeue(id,isOk);
        if(isOk) sumConsumed++;
        return val;
    }
    bool  Enqueue    (const Key& id,Value value                      )
    {
        bool rv = false;
        if(MultiQueueProcessor<Key,Value>::Enqueue(id,value))
        {
            rv = true;
            ++summAdded;
        }
        return rv;
    }
    void StopProcessing (           )
    {
        MultiQueueProcessor<Key,Value>::StopProcessing(); end = std::chrono::steady_clock::now();
        elapsed  = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
        std::cerr <<  boost::format("summAdded = %1% sumConsumed= %2% sumSubscribed = %3% \n time elapsed:%4% (ms) \n") % summAdded % sumConsumed % sumSubscribed % elapsed.count();
    }
    bool  Unsubscribe(const Key& id                                  )
    {
        bool rv = false;
        if(MultiQueueProcessor<Key,Value>::Unsubscribe(id))
        {
            sumUnsubscribed++;
            rv = true;
        }
        return rv;
    }
    bool  Subscribe  (const Key& id, IConsumer<Key, Value> * consumer)
    {
        bool rv = false;
        if(MultiQueueProcessor<Key,Value>::Subscribe(id,consumer))
        {
            rv = true;
            sumSubscribed++;
        }
        return rv;
    }

    std::atomic_int summAdded         = 0;
    std::atomic_int sumConsumed       = 0;
    std::atomic_int sumSubscribed     = 0;
    std::atomic_int sumUnsubscribed   = 0;
    std::chrono::steady_clock::time_point begin;
    std::chrono::steady_clock::time_point end  ;
    std::chrono::milliseconds elapsed    ;
};


//cntPr - count thread of Productions
//elemPerProd  - count element per productor
//cntCons - count thread of consumers
//qs - vector with queue Keys
//secs - duration of estimate
template<typename Key, typename Value>
void ProdTest(int cntPr,int elemPerProd, int cntCons, std::vector<Key>& qs, float secs)
{
    if(qs.size())
    {
        auto beginInit = std::chrono::steady_clock::now();
        MQPTest<Key,Value>* myproc  = new MQPTest<Key,Value>;
        std::vector<IConsumer<Key,Value>*> mycons(cntCons);
        for(auto& it:mycons) it = new IConsumer<Key,Value>();

        std::random_shuffle ( mycons.begin(), mycons.end() );
        std::random_shuffle ( qs.begin()    , qs.end()     );

        std::recursive_mutex addProc; //mutex for producers
        std::recursive_mutex addCons; //mutex for consumers


        std::thread prods[cntPr  ];
        std::thread conss[cntCons];

        std::time_t now = std::time(0);
        std::mt19937 gen{static_cast<std::uint32_t>(now)};
        std::uniform_int_distribution<> dist{0, int(qs.size() - 1) };

        auto fproducers = [&myproc,&qs,&dist,&gen,&addProc,&elemPerProd]() -> void
        {
            int cnt = elemPerProd;
            while(cnt--)
            {
                int randKey = dist(gen);
                std::scoped_lock<std::recursive_mutex> lock(addProc);
                if(myproc && myproc->isRunning()) myproc->Enqueue(qs[randKey],Value());
            }
        };

        auto fconsumers = [&myproc,&qs,&mycons,&dist,&gen,&addProc,&addCons]() -> void
        {

            int cnt = 5;
            while(cnt--)
            {
                for(auto& it : mycons)
                {
                    int randKey = dist(gen);
                    std::scoped_lock<std::recursive_mutex,std::recursive_mutex> lock{addProc,addCons};
                    if(myproc && myproc->isRunning()) myproc->Subscribe  (qs[randKey],it);
                }
            }

            std::scoped_lock<std::recursive_mutex> lock(addProc);
            {
                int radKey = dist(gen);
                if(myproc && myproc->isRunning()) myproc->Unsubscribe  (qs[radKey]);
            }

        };


        auto endInit = std::chrono::steady_clock::now();
        auto elapsInit = std::chrono::duration_cast<std::chrono::milliseconds>(endInit - beginInit);
        for(auto && it : prods) it = std::thread(fproducers);
        for(auto && it : conss) it = std::thread(fconsumers);

        std::this_thread::sleep_for(std::chrono::seconds(int(secs)));
        {
            myproc->StopProcessing();
            std::cerr << "Pure work time :elapsed_ms :" << (myproc->elapsed.count() - elapsInit.count()) << std::endl;
            std::scoped_lock<std::recursive_mutex> lock(addProc);
            delete myproc;
            myproc = nullptr;
        }

        for(auto&it :  prods)  it.join();
        for(auto&it :  conss)  it.join();
        for(auto it : mycons)  delete it;
    }
};
