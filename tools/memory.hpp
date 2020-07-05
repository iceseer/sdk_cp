namespace memory {

template<typename __type, typename __alloc, typename...__args>
std::unique_ptr<__type, std::function<void(__type*)>> sdk_cp_make_unique(__alloc& alloc, __args&&...args) {
    using allocator_type = typename __alloc::template rebind<__type>::other;
    allocator_type tmp_alloc(alloc);

    auto deleter = [](__type *p, allocator_type alloc) {
        alloc.destroy(p);
        alloc.deallocate(p, 1);
    };

    auto* const ptr = tmp_alloc.allocate(1);
    tmp_alloc.construct(ptr, std::forward<__args>(args)...);

    return { ptr, std::bind(deleter, std::placeholders::_1, tmp_alloc) };
}

}