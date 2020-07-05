///////////////////////////
// author: Alexander Lednev
// date: 19.05.2020
///////////////////////////
template<typename T, typename _> class StrongType final {
    T _t;
public:
    StrongType()
    { }
    StrongType(T const& t) : _t(t)
    { }
    StrongType(T&& t) : _t(std::move(t))
    { }
    StrongType(StrongType const& t) : _t(t._t)
    { }
    StrongType(StrongType&& t) : _t(std::move(t._t))
    { }
    
    StrongType& operator=(T const& t) {
        _t = t;
        return *this;
    }
    StrongType& operator=(T&& t) {
        _t = std::move(t);
        return *this;
    }
    StrongType& operator=(StrongType const& t) {
        _t = t._t;
        return *this;
    }
    StrongType& operator=(StrongType&& t) {
        _t = std::move(t._t);
        return *this;
    }

    T& operator->() { return _t; }
};

#define STRONG_TYPE(type, name) using name = tools::StrongType<type, struct name ## _tag>;