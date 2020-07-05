///////////////////////////
// author: Alexander Lednev
// date: 14.02.2017
///////////////////////////
#define __USE_IN_PAGE_ALLOCATION__

#ifdef SDK_CP_DEBUG
#   define __USE_PAGE_VALIDATION__
#endif//SDK_CP_DEBUG

#ifndef ASSERT_INTERNAL
#   define ASSERT_INTERNAL(x) SDK_CP_ASSERT(x)
#endif//ASSERT_INTERNAL

#if !defined(__VALIDATE_ACTIVE_PAGE__) && defined(__USE_PAGE_VALIDATION__)
#   ifdef _DEBUG
#       define __VALIDATE_ACTIVE_PAGE__ \
            if (_active_position) { \
                const uint32_t __position__ = _getPageIndex(_active_position); \
                const uint32_t __blockIx__ = (__position__ >> k_flag_bound); \
                const uint32_t __bitPosition__ = (__position__ & k_l_mask); \
                block_type __block__ = _blocks[__blockIx__]; \
                __block__ &= (1llu << __bitPosition__); \
                if (__block__ && uint32_t(_active_position - _active_page) < __page_size) { \
                    __debugbreak(); \
                } \
            }
#   else
#       define __VALIDATE_ACTIVE_PAGE__
#   endif//_DEBUG
#   else
#       define __VALIDATE_ACTIVE_PAGE__
#endif//__VALIDATE_ACTIVE_PAGE__

#define FLAG_TYPE_MAX SDK_CP_UINT64_MAX
#define BSF_INST tools::processor::bit_scan_forward_64
static_assert(sizeof(size_t) == 8, "this code support only x64");

namespace allocators
{
    const uint64_t st_mask[] =
    {
        0xffffffffffffffff, 0xfffffffffffffffe, 0xfffffffffffffffc, 0xfffffffffffffff8, 0xfffffffffffffff0,
        0xffffffffffffffe0, 0xffffffffffffffc0, 0xffffffffffffff80, 0xffffffffffffff00, 0xfffffffffffffe00,
        0xfffffffffffffc00, 0xfffffffffffff800, 0xfffffffffffff000, 0xffffffffffffe000, 0xffffffffffffc000,
        0xffffffffffff8000, 0xffffffffffff0000, 0xfffffffffffe0000, 0xfffffffffffc0000, 0xfffffffffff80000,
        0xfffffffffff00000, 0xffffffffffe00000, 0xffffffffffc00000, 0xffffffffff800000, 0xffffffffff000000,
        0xfffffffffe000000, 0xfffffffffc000000, 0xfffffffff8000000, 0xfffffffff0000000, 0xffffffffe0000000,
        0xffffffffc0000000, 0xffffffff80000000, 0xffffffff00000000, 0xfffffffe00000000, 0xfffffffc00000000,
        0xfffffff800000000, 0xfffffff000000000, 0xffffffe000000000, 0xffffffc000000000, 0xffffff8000000000,
        0xffffff0000000000, 0xfffffe0000000000, 0xfffffc0000000000, 0xfffff80000000000, 0xfffff00000000000,
        0xffffe00000000000, 0xffffc00000000000, 0xffff800000000000, 0xffff000000000000, 0xfffe000000000000,
        0xfffc000000000000, 0xfff8000000000000, 0xfff0000000000000, 0xffe0000000000000, 0xffc0000000000000,
        0xff80000000000000, 0xff00000000000000, 0xfe00000000000000, 0xfc00000000000000, 0xf800000000000000,
        0xf000000000000000, 0xe000000000000000, 0xc000000000000000, 0x8000000000000000
    };
    static_assert(sizeof(size_t) == 8, "this code support only x64");

    template<uint32_t __init_size, uint32_t __page_size = SDK_CP_PAGE_SIZE> class heap_allocator
    {
        static_assert(__init_size > 0,                        "__init_size must be > 0");
        static_assert(__page_size > 1024,                     "__page_size must be > 1024");
        static_assert(__page_size <= 16384,                   "__page_size must be <= 16384");
        static_assert((__page_size & (__page_size - 1)) == 0, "__page_size must be pow 2");

        typedef uint64_t block_type;
        struct alloc_header {
            uint64_t size;
            uint8_t __unused[SDK_CP_ALIGNMENT - sizeof(uint64_t)];
        };

        enum { k_flag_bound = 6 };
        static_assert(sizeof(alloc_header) < __page_size, "sizeof alloc_header must be < __page_size");

        enum { __page_mask = __page_size - 1 };

        enum { k_h_val = (1 << k_flag_bound) };
        enum { k_l_mask = (k_h_val - 1) };

        enum { __NP = __init_size / __page_size };
        enum { __NB = __NP / k_h_val + ((__NP & k_l_mask) ? 1 : 0) };

        enum { __num_blocks = __NB };
        enum { __num_pages  = (__num_blocks << k_flag_bound) };
        static_assert(__num_pages > 0, "__init_size must be > __page_size");

        enum { __allocation_size = __num_pages * __page_size };
        enum { __invalid_page_ix = -1 };

#       pragma pack(push, 2)
        struct page_info {
            uint16_t usage;
        };
#       pragma pack(pop)

        uint8_t  _mem_ptr_raw[__allocation_size + __page_size - 1];
        uint8_t* _mem_ptr;

        SDK_CP_ALIGN(block_type,_blocks[__num_blocks],   4);
        SDK_CP_ALIGN(page_info, _pagesInfo[__num_pages], 2);

        uint8_t* _active_position;
        uint8_t* _active_page;

        inline uint8_t* _getPage(uint32_t const ix) {
            ASSERT_INTERNAL(ix < __num_pages);
            if (ix < __num_pages) {
                return _mem_ptr + (__page_size * ix);
            }
            return nullptr;
        }

        inline uint32_t _getPageIndex(uint8_t* mem_ptr) const {
            if (mem_ptr != nullptr) {
                uint32_t const ix = uint32_t((mem_ptr - _mem_ptr) / __page_size);
                ASSERT_INTERNAL(ix < __num_pages);
                return ix;
            }
            return (uint32_t)__invalid_page_ix;
        }

        inline void _resetBitStream(uint32_t const from, uint32_t const count) {
            uint32_t const blockIx = (from >> k_flag_bound);
            uint32_t position      = (from & k_l_mask);

            uint32_t remains = count;
            block_type* block = &_blocks[blockIx];
            while (remains > 0) {
                const block_type b1 = (1llu << position);
                const uint32_t e0 = position + remains;
                const block_type e1 = ((e0 >= k_h_val) ? 0 : (1llu << e0));
                const block_type blockEnd = (e1 - 1);

                const block_type blockStart = /*st_mask[position];*/~(b1 - 1);
                const block_type blockMask = blockStart & blockEnd;
                *block &= ~blockMask;

                remains -= sdk_cp_min(e0, (uint32_t)k_h_val) - position;
                position = 0;
                ++block;
            }
        }

        inline void _setBitStream(uint32_t const from, uint32_t const count) {
            uint32_t const blockIx = (from >> k_flag_bound);
            uint32_t position      = (from & k_l_mask);

            uint32_t remains = count;
            block_type* block = &_blocks[blockIx];
            while (remains > 0) {
                const block_type b1 = (1llu << position);
                const uint32_t e0 = position + remains;
                const block_type e1 = ((e0 >= k_h_val) ? 0 : (1llu << e0));
                const block_type blockEnd = (e1 - 1);

                const block_type blockStart = /*st_mask[position];*/~(b1 - 1);
                const block_type blockMask = blockStart & blockEnd;
                *block |= blockMask;

                remains -= sdk_cp_min(e0, (uint32_t)k_h_val) - position;
                position = 0;
                ++block;
            }
        }

        inline uint32_t _getBitScanStreamRevWord(uint32_t const numberOfSetBits) {
            uint32_t remainsFindPages = numberOfSetBits;
            uint32_t startPage        = (uint32_t)__invalid_page_ix;

            uint32_t blockIx = 0;
            while (remainsFindPages > 0 && blockIx < __num_blocks) {
                block_type block = _blocks[blockIx];
                uint64_t position;

                for (;;) {
                    position = BSF_INST(block);
                    if (SDK_CP_UINT64_MAX != position) {
                        if (startPage != (uint32_t)__invalid_page_ix) {
                            const block_type bound = ((remainsFindPages >= k_h_val) ? 0 : (1llu << remainsFindPages));
                            const block_type boundMask = (bound - 1);

                            if ((block & boundMask) == boundMask) {
                                remainsFindPages -= sdk_cp_min(remainsFindPages, (uint32_t)k_h_val);
                                ++blockIx;
                                break;
                            }

                            const block_type b1 = block - 1;
                            const block_type b2 = block | b1;
                            const block_type b3 = b2 + 1;
                            block &= b3;

                            remainsFindPages = numberOfSetBits;
                            startPage = (uint32_t)__invalid_page_ix;
                            continue;
                        }

                        const block_type blockMask = st_mask[position];//(t1 & t3) | t2;

                        const uint32_t endPosition = uint32_t(position + remainsFindPages);
                        const block_type bound = ((endPosition >= k_h_val) ? 0 : (1llu << endPosition));
                        const block_type boundMask = (bound - 1);

                        const block_type normalizedBlock = (blockMask & boundMask);
                        if ((block & normalizedBlock) == normalizedBlock) {
                            startPage = (blockIx << k_flag_bound) + (uint32_t)position;
                            remainsFindPages -= sdk_cp_min(endPosition, (uint32_t)k_h_val) - (uint32_t)position;
                            ++blockIx;
                            break;
                        }
                        else {
                            const block_type b1 = block - 1;
                            const block_type b2 = block | b1;
                            const block_type b3 = b2 + 1;
                            block &= b3;
                            continue;
                        }
                    }
                    else {
                        remainsFindPages = numberOfSetBits;
                        startPage = (uint32_t)__invalid_page_ix;
                        ++blockIx;
                        break;
                    }
                }
            }

            if (remainsFindPages > 0) {
                startPage = (uint32_t)__invalid_page_ix;
            }

            return startPage;
        }

    public:
        inline heap_allocator()
            : _active_position(nullptr)
            , _active_page(nullptr)
        {
            ::memset(_blocks, 0xff, sizeof(_blocks));
            ::memset(_pagesInfo, 0, sizeof(_pagesInfo));

            const uintptr_t dif = uintptr_t(_mem_ptr_raw) & __page_mask;
            if (dif > 0) {
                _mem_ptr = (uint8_t*)(_mem_ptr_raw + (__page_size - dif));
            }
            else {
                _mem_ptr = _mem_ptr_raw;
            }
        }

        ~heap_allocator()
        {
        }

        uintptr_t get_memory_begin() const {
            return uintptr_t(_mem_ptr);
        }
        uintptr_t get_memory_end() const {
            return uintptr_t(_mem_ptr + __allocation_size);
        }

        uint8_t* allocate(uint32_t size)
        {
            ASSERT_INTERNAL(_mem_ptr != nullptr);
            const uint32_t mask = SDK_CP_ALIGNMENT - 1;
            size += mask & (SDK_CP_ALIGNMENT - (size & mask));

            uint8_t* ptr = nullptr;
            if (_mem_ptr != nullptr && size > 0) {
                const uint64_t needSize = size + sizeof(alloc_header);

                uint32_t fullPagesCount = uint32_t(needSize / __page_size);
#ifdef __USE_IN_PAGE_ALLOCATION__
                if (!fullPagesCount && _active_position != nullptr && _active_page != nullptr) {
                    const uint32_t fill = uint32_t(_active_position - _active_page);
                    const uint32_t remains = __page_size - fill;
                    ASSERT_INTERNAL(remains <= __page_size);

                    if (remains >= needSize) {
                        const uint32_t position = _getPageIndex(_active_page);
                        ASSERT_INTERNAL(position <= __num_pages);

                        if (position < __num_pages) {
                            alloc_header* header = (alloc_header*)_active_position;
                            header->size = size;
                            ptr = _active_position + sizeof(alloc_header);

                            page_info& page = _pagesInfo[position];
                            page.usage += (uint16_t)needSize;
                            _active_position += needSize;

                            ASSERT_INTERNAL(page.usage <= __page_size);
                            ASSERT_INTERNAL(_active_position - _active_page <= __page_size);

                            __VALIDATE_ACTIVE_PAGE__;
                            return ptr;
                        }
                    }

                    _active_position = nullptr;
                    _active_page = nullptr;
                }
#endif//__USE_IN_PAGE_ALLOCATION__

                const uint32_t _usage = (needSize & __page_mask);
                const uint16_t lastPageUsage = uint16_t(_usage + (__page_size * (!_usage)));
                fullPagesCount += ((needSize & __page_mask) ? 1 : 0);
                const uint32_t position = _getBitScanStreamRevWord(fullPagesCount);
                if (position != (uint32_t)__invalid_page_ix) {
                    const uint32_t lastPageIx = position + fullPagesCount - 1;
                    //page_info& startPage = _pagesInfo[position];
                    //startPage.usage = __page_size;

                    page_info& lastPage = _pagesInfo[lastPageIx];
                    lastPage.usage = lastPageUsage;

                    uint8_t* pageStart = _getPage(position);
                    ASSERT_INTERNAL(pageStart != nullptr);

                    if (pageStart != nullptr) {
                        uint8_t* pageLast = _getPage(lastPageIx);
                        ASSERT_INTERNAL(pageLast != nullptr);

                        if (pageLast != nullptr) {
                            alloc_header* header = (alloc_header*)(pageStart);
                            header->size = size;
                            ptr = pageStart + sizeof(alloc_header);

                            if (_active_position == nullptr) {
                                _active_page = pageLast;
                                _active_position = _active_page + lastPageUsage;
                            }
                            _resetBitStream(position, fullPagesCount);
                        }
                    }
                }
            }

            __VALIDATE_ACTIVE_PAGE__;
            //ASSERT_INTERNAL(ptr != nullptr);
            return ptr;//FFFFE701
        }

        bool release(uint8_t* mem)
        {
            bool result = false;
            for (;;) {
                ASSERT_INTERNAL(mem != nullptr);
                if (mem == nullptr) {
                    break;
                }

                ASSERT_INTERNAL(_mem_ptr != nullptr);
                if (_mem_ptr == nullptr) {
                    break;
                }

                uint32_t position = _getPageIndex(mem);
                ASSERT_INTERNAL(position < __num_pages);
                if (mem < _mem_ptr || position >= __num_pages) {
                    break;
                }

                result = true;
                alloc_header& header = *(alloc_header*)(mem - sizeof(alloc_header));
                ASSERT_INTERNAL(header.size > 0);
                if (header.size == 0) {
                    break;
                }

                uint64_t needSize = header.size + sizeof(alloc_header);

                if (needSize > __page_size) {
                    const uint32_t fullPagesCount = uint32_t(needSize / __page_size) + ((needSize & __page_mask) ? 1 : 0);
                    const uint32_t lastPageIx = position + fullPagesCount - 1;

                    _setBitStream(position, fullPagesCount - 1);
                    const uint32_t _usage = (needSize & __page_mask);
                    const uint16_t lastPageUsage = uint16_t(_usage + (__page_size * (!_usage)));

                    needSize = lastPageUsage;
                    position = lastPageIx;
                }

                page_info& page = _pagesInfo[position];
                ASSERT_INTERNAL(page.usage >= needSize);
                page.usage -= (uint16_t)needSize;

                if (page.usage == 0) {
                    const uint32_t blockIx = (position >> k_flag_bound);
                    const uint32_t bitPosition = (position & k_l_mask);

                    block_type& block = _blocks[blockIx];
                    block |= (1llu << bitPosition);

                    if (_getPageIndex(_active_page) == position) {
                        _active_page = nullptr;
                        _active_position = nullptr;
                    }
                }

                break;
            }

            __VALIDATE_ACTIVE_PAGE__;
            return result;
        }
    };
}
