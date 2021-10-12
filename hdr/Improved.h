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
#include <atomic>
#include <cassert>
#include <fstream>

//########### OUTPUT #########
//Camera order#001 processed.
//Tripod order#002 processed.
//Camera order#003 processed.
//Lens order#004 processed.
//Lens order#005 processed.
//Camera order#006 processed.
//Tripod order#007 processed.
//Lens order#008 processed.
//Tripod order#009 processed.
//Camera order#010 processed.
//Tripod order#011 processed.
//Dummy Order: order#012 handled by [default visitor]
//###########################

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

// Need to seperate out "Manufacturing" process and "Order" Object. Maybe another feature would need to add some other operation with "Order" So per SOLID principle
// Class is closed for modification Open for extension.

class Visitor;

class Order{

public:
    Order(std::string id, std::size_t deadline)
        : m_id(std::move(id)), m_deadline(deadline) {}

    friend bool operator<(const std::shared_ptr<Order>& lhs, const std::shared_ptr<Order>& rhs){
        return (rhs->m_deadline < lhs->m_deadline);
    }

    std::string GetID()  const { return m_id; }
    std::size_t GetDeadline()  const { return m_deadline; }

    virtual void accept(Visitor& v) = 0;

protected:
    std::string m_id;
    std::size_t m_deadline;
};

class Camera;
class Tripod;
class Lens;

// Any visitor must support operation on all Order types.
class Visitor {
public:
    virtual void visit(const Camera& c) const = 0;
    virtual void visit(const Tripod& t) const = 0;
    virtual void visit(const Lens& l) const = 0;
};

// Maybe packing task??
class DefaultVisitor {
public:
    static void visit(const Order& c){
        std::cout << "Dummy Order: " << c.GetID() << " handled by [default visitor]" << std::endl;
    }
};

class OrderManufacturingVisitor;

// CRTP for performance optimazation on runtime polymorphism.
template <typename Derived>
class Visitable : public Order {

public:
    using Order::Order;

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

    void accept(Visitor& v) override{

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

// Seperate Object creation and Algorithm which works with it.
class OrderManufacturingVisitor : public Visitor{

public:
    void visit(const Camera& c) const override{
        std::cout << "Camera " << c.GetID() << " processed." << std::endl;
    }
    void visit(const Tripod& t) const override{
        std::cout << "Tripod " << t.GetID() << " processed." << std::endl;
    }
    void visit(const Lens& l) const override{
        std::cout << "Lens " << l.GetID() << " processed." << std::endl;
    }
};

// Factory pattern to create object without exposing the creation logic to the client. Strong exception safety guranteed
class Factory{

public:
    static std::shared_ptr<Order> GetOrder(const std::string& orderData) noexcept{

        // SSO
        static const std::string CameraTag{"Camera"};
        static const std::string TripodTag{"Tripod"};
        static const std::string LensTag{"Lens"};
        static const std::string DummyTag{"Dummy"};

        std::vector<std::string> v = Split(orderData, ' ');
        if (v.size() != 3)
            return nullptr;

        try{
            const auto& orderType = v[1];
            if (orderType == CameraTag){
                return std::make_shared<Camera>(std::move(v[0]), (std::size_t)std::stoi(v[2].data()));
            }
            if (orderType == TripodTag){
                return std::make_shared<Tripod>(std::move(v[0]), (std::size_t)std::stoi(v[2].data()));
            }
            if (orderType == LensTag){
                return std::make_shared<Lens>(std::move(v[0]), (std::size_t)std::stoi(v[2].data()));
            }
            if (orderType == DummyTag){
                return std::make_shared<DummyOrder>(std::move(v[0]), (std::size_t)std::stoi(v[2].data()));
            }
        }catch(std::exception& e){
            std::cerr << "Input error" << e.what() << std::endl;
            return nullptr;
        }

        assert(false && "new Order Factory not aware!!");
    }
};

class OrderProcessor{

public:
    void AddOrder(const std::shared_ptr<Order> o){
        std::lock_guard<std::mutex> lk(m_Mu);
        m_SortedOrder.push(o);
    }

    bool Process(){

        while(true){

            while(!m_SortedOrder.empty()){
                {
                    std::lock_guard<std::mutex> lk(m_Mu);
                    const std::shared_ptr<Order>& visitable = m_SortedOrder.top();
                    if(visitable){
                        visitable->accept(v);
                    }
                    m_SortedOrder.pop();
                }
            }
            if (m_Exit)
                break;
        }

        return m_SortedOrder.empty();
    }

    void SetExit(){
        m_Exit = true;
    }

private:
    std::mutex m_Mu;
    OrderManufacturingVisitor v;
    std::priority_queue<std::shared_ptr<Order>> m_SortedOrder;
    std::atomic_bool m_Exit{false};
};

class InputParser{

public:
    static void ParseInput(const std::string_view file_name, OrderProcessor& op){
        std::ifstream input(file_name.data());
        if (!input){
            std::cerr << "input file not found" << std::endl;
            return;
        }
        for (std::string line; std::getline(input, line, '\n'); ){
            // maybe std::yield after several lines of processing
            if (!line.empty()){
                const std::shared_ptr<Order> o(Factory::GetOrder(line));
                if (o){
                    op.AddOrder(o);
                }
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
    auto fu = std::async(std::launch::async, &OrderProcessor::Process, &op);
    const std::string_view input_file(argv[1]);
    InputParser::ParseInput(input_file, op);

    // Process order
    op.SetExit();
    if (!fu.get()){
        op.Process();
    }
}
