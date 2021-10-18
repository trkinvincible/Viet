#pragma once

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <future>
#include <string>
#include <mutex>
#include <sstream>
#include <chrono>
#include <cassert>
#include <fstream>
#include <algorithm>

//########### OUTPUT #########
//##Processing Batch: 1
//Camera order#001 processed.priority: 1
//Processed by batch: 1
//Tripod order#002 processed.priority: 2
//Processed by batch: 1
//Lens order#005 processed.priority: 3
//Processed by batch: 1
//Camera order#003 processed.priority: 9
//Processed by batch: 1
//Lens order#004 processed.priority: 15
//Processed by batch: 1

//##Processing Batch: 2
//Camera order#006 processed.priority: 1
//Processed by batch: 2
//Tripod order#009 processed.priority: 2
//Processed by batch: 2
//Camera order#010 processed.priority: 5
//Processed by batch: 2
//Tripod order#007 processed.priority: 7
//Processed by batch: 2
//Lens order#008 processed.priority: 99
//Processed by batch: 2

//##Processing Batch: 3
//Tripod order#011 processed.priority: 3
//Processed by batch: 3
//Dummy Order: order#012 handled by [default visitor]
//Processed by batch: 3
//###########################

namespace Controller {
    class Visitor;
}

namespace Model {

    using namespace Controller;

    // Need to seperate out "Manufacturing" process and "Order" Object. Maybe another feature would need to add some other operation with "Order" So per SOLID principle
    // Class is closed for modification Open for extension.
    class Order{

    public:
        virtual ~Order() {
            m_id.clear();
        }

        Order(std::string &&id, std::size_t deadline)
            : m_id(std::move(id)), m_deadline(deadline) {}

        friend bool operator>(const Order& lhs, const Order& rhs){
            return (lhs.m_deadline > rhs.m_deadline);
        }

        std::string GetID()  const { return m_id; }
        std::size_t GetDeadline()  const { return m_deadline; }

        virtual void accept(const Visitor& v) = 0;

    protected:
        std::string m_id;
        std::size_t m_deadline;
    };

    using OrderPtr = std::unique_ptr<Model::Order>;

    // CRTP for performance optimazation on runtime polymorphism.
    template <typename Derived>
    class Visitable : public Order {

        using Order::Order;
    public:
        template<typename visitorType, typename orderType, typename = bool >
        struct has_ValidVisit
          : std::false_type
        {};

        template<typename visitorType, typename orderType>
        struct has_ValidVisit<visitorType, orderType, typename std::enable_if_t<
                                                                   std::is_same_v<
                                                                   decltype(std::declval<visitorType>().visit(std::declval<orderType>())),
                                                                   void >,
                                                               bool>>
          : std::true_type
        {};

        void accept(const Visitor& v) override;
    };

    class DummyOrder : public Visitable<DummyOrder> {
        using Visitable<DummyOrder>::Visitable;
    };

    class Camera : public Visitable<Camera> {
        using Visitable<Camera>::Visitable;
    };

    class Tripod : public Visitable<Tripod> {
        using Visitable<Tripod>::Visitable;
    };

    class Lens : public Visitable<Lens> {
        using Visitable<Lens>::Visitable;
    };
}

namespace Controller {

    using namespace Model;

    // Any visitor must support operation on all Order types.
    class Visitor {
    public:
        virtual void visit(const Camera& c) const = 0;
        virtual void visit(const Tripod& t) const = 0;
        virtual void visit(const Lens& l) const = 0;
    };

    // Maybe packing or transport task??
    class DefaultVisitor {
    public:
        static void visit(const Model::Order& c){
            std::cout << "Dummy Order: " << c.GetID() << " handled by [default visitor]" << std::endl;
        }
    };

    class OrderManufacturingVisitor : public Visitor{

    public:
        void visit(const Camera& c) const override{
            std::cout << "Camera " << c.GetID() << " processed." << "priority: " << c.GetDeadline() << std::endl;
        }
        void visit(const Tripod& t) const override{
            std::cout << "Tripod " << t.GetID() << " processed." << "priority: " << t.GetDeadline() << std::endl;
        }
        void visit(const Lens& l) const override{
            std::cout << "Lens " << l.GetID() << " processed." << "priority: " << l.GetDeadline() << std::endl;
        }
    };
}

namespace Model {

    template <typename Derived>
    void Visitable<Derived>::accept(const Visitor& v) {

        using visitorType = typename std::remove_reference_t<decltype(v)>;
        using orderType = typename std::remove_pointer_t<decltype(static_cast<Derived*>(this))>;
        //using newType = typename std::enable_if_t<std::is_same_v<decltype(std::declval<visitorType>().visit(std::declval<orderType>())), void>, bool>;
        //static_assert(std::is_same_v<newType, bool>, "failed");

        if constexpr (has_ValidVisit<visitorType, orderType>()){
            v.visit(static_cast<Derived&>(*this));
        }else{
            DefaultVisitor::visit(static_cast<Derived&>(*this));
        }
    }
}

namespace Util {

    using namespace Model;
    using namespace Controller;

    std::vector<std::string> Split(const std::string& input, const char delimiter){

        std::vector<std::string> result;
        std::stringstream ss(input);
        std::string s;
        while (std::getline(ss, s, delimiter)) {
            if (!s.empty())
                result.push_back(s);
        }

        return result;
    }

    // Factory pattern to create object without exposing the creation logic to the client. Strong exception safety guranteed
    class Factory{

    public:
        static OrderPtr GetOrder(const std::string& orderData) noexcept{

            // SSO
            static constexpr auto CameraTag{"Camera"};
            static constexpr auto TripodTag{"Tripod"};
            static constexpr auto LensTag{"Lens"};
            static constexpr auto DummyTag{"Dummy"};

            std::vector<std::string> v = Split(orderData, ' ');
            if (v.size() != 3)
                return nullptr;

            try{
                const auto& orderType = v[1];
                if (orderType == CameraTag){
                    return std::make_unique<Camera>(std::move(v[0]), (std::size_t)std::stoi(v[2].data()));
                }
                else if (orderType == TripodTag){
                    return std::make_unique<Tripod>(std::move(v[0]), (std::size_t)std::stoi(v[2].data()));
                }
                else if (orderType == LensTag){
                    return std::make_unique<Lens>(std::move(v[0]), (std::size_t)std::stoi(v[2].data()));
                }
                else if (orderType == DummyTag){
                    return std::make_unique<DummyOrder>(std::move(v[0]), (std::size_t)std::stoi(v[2].data()));
                }
            }catch(const std::exception& e){
                std::cerr << "Input error" << e.what() << std::endl;
                return nullptr;
            }

            assert(false && "new Order Factory not aware!!");
        }
    };
}

using namespace Util;
namespace std {
    template <>
    struct greater<unique_ptr<Model::Order>> {
        using OrderType = Model::Order;
        bool operator()(const unique_ptr<OrderType>& lhs, const unique_ptr<OrderType>& rhs) const {
          return *lhs > *rhs;
        }
    };
}

class OrderProcessor{

    using ProcessingStoreType = std::priority_queue<Model::OrderPtr, std::vector<Model::OrderPtr>, std::greater<Model::OrderPtr>>;

public:
    // Dictates send me only RValue ref because i ll move it into my world henceforth.
    void AddOrder(OrderPtr&& o){
        static constexpr bool test = std::is_same_v<decltype(operator>(std::declval<const Order&>(), std::declval<const Order&>())), bool>;
        static_assert (test, "Order class (T) must implement operator>()");

        m_Store.emplace(std::move(o));
        if (m_Store.size() == BATCH_SIZE){
            Process();
        }
    }

    void Process(){
        static int count = 0;
        {
            std::lock_guard<std::mutex> lk(m_CoutMutex);
            std::cout << "##Processing Batch: " << ++count << std::endl;
        }
        auto fu = std::async(std::launch::async, [thisBatch = count, this, store = std::move(m_Store)]() mutable{
            while(!store.empty()){
                const OrderPtr& visitable = store.top();
                {
                    std::lock_guard<std::mutex> lk(m_CoutMutex);
                    visitable->accept(v);
                    std::cout << "Processed by batch: " << thisBatch << std::endl;
                }
                store.pop();
            }
            return true;
        });

        m_Futures.push_back(std::move(fu));
    }

    void SetExit(){
        if (!m_Store.empty()){
            Process();
        }
        std::for_each(m_Futures.begin(), m_Futures.end(),[](std::future<bool> &fu){
            fu.get();
        });
    }

private:
    std::mutex m_CoutMutex;
    OrderManufacturingVisitor v;
    ProcessingStoreType m_Store;
    std::vector<std::future<bool>> m_Futures;
    static constexpr int BATCH_SIZE = 5;
};

class InputParser{

public:
    static void ParseInput(const std::string_view file_name, OrderProcessor& op){
        std::ifstream input(file_name.data());
        if (!input){
            std::cerr << "input file not found" << std::endl;
            return;
        }
        for (std::string line; std::getline(input, line, '\n');){

            if (!line.empty()){
                op.AddOrder(Factory::GetOrder(line));
            }
        }
    }
};

void mainlocal(int argc, char **argv){

    if (argc < 2){
        std::cerr << "Usage: ./solution /path/to/order.txt" << std::endl;
        return;
    }

    OrderProcessor op;
    InputParser::ParseInput(argv[1], op);
    op.SetExit();
}
