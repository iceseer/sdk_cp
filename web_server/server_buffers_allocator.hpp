namespace tools {
    template<> class objects_cache_default_allocator<evbuffer> {
    public:
        template<typename...__args> struct evbuffer* allocate(__args&.../*args*/) {
            return evbuffer_new();
        }
        void deallocate(struct evbuffer* obj) {
            evbuffer_free(obj);
        }
    };
}
