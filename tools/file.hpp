namespace file {

    typedef std::shared_ptr<FILE> file_handle;

    inline file_handle open_file(char const* path, char const* mode) {
        return file_handle(fopen(path, mode), [](FILE* file) {
            if (nullptr != file)
                fclose(file);
        });
    }

    inline bool write_file(void const* data, uint64_t const size, uint64_t const count, file_handle& file) {
        if (!!file)
            return count == fwrite(data, size, count, file.get());
        return false;
    }

    template<typename __string_type>
    inline bool read_all_file(__string_type& data, file_handle& file) {
        if (!!file) {
            auto fh = file.get();
            fseek(fh, 0, SEEK_END);
            data.resize(ftell(fh));
            rewind(fh);
            fseek(fh, 0, SEEK_SET);
            if (!data.empty()) {
                size_t accum = 0;
                do {
                    accum += fread(&data[0], 1, data.size() - accum, fh);
                } while (accum < data.size() /*&& !feof(fh)*/);
                //data[accum] = 0;
                return data.size() == accum;
            }
        }
        return false;
    }

    inline void print_buffer(file_handle& file, char k, char const* mark, uint8_t const* buf_beg, uint8_t const* buf_end, uint8_t const* target_beg, uint8_t const* target_end) {
        auto const buffer_size = buf_end - buf_beg;
        std::string out;
        out.reserve(buffer_size + 16);
        uint8_t const* ptr = buf_beg;

        out += mark;
        while(ptr != buf_end) {
            if (ptr >= target_beg && ptr < target_end) {
                out += k;
            } else {
                out += '_';
            }
            ++ptr;
        }
        out += '\n';
        write_file(out.c_str(), out.size(), 1, file);
    }

    SDK_CP_ALWAYS_INLINE bool create_dir(char const* path) {
        SDK_CP_ASSERT(nullptr != path);
        SDK_CP_ASSERT(0 != sdk_cp_strlen(path));

#ifdef _WIN32
        std::filesystem::path p(path);
        return std::filesystem::create_directory(p);
#else
        return false;
#endif//_WIN32
    }

    template<size_t __sz> SDK_CP_ALWAYS_INLINE bool working_dir(char(&buf)[__sz]) {
        if (auto ptr =
#   ifdef __GNUG__
            getcwd(buf, __sz)
#   elif _WIN32
            _getcwd(buf, __sz)
#   endif
            ) {
            return true;
        }

#ifdef _WIN32
        tools::log::error("'getcwd' return error: ", strerror(errno));
#endif//_WIN32
        return false;
    }

    template<size_t __sz> SDK_CP_ALWAYS_INLINE bool current_dir(char(&buf)[__sz]) {
#   ifdef __GNUG__
        return false;
#   elif _WIN32
        static_assert(__sz >= MAX_PATH, "Too small buffer.");
        auto res = GetModuleFileNameA(nullptr, buf, __sz);
        if (!res) {
            tools::log::error("Get module path failed: ", GetLastError());
            return false;
        }
        *((char*)r_substr(buf, { '\\', '/' })) = 0;
#   endif
        return true;
    }

#ifdef _WIN32
    template<typename __func, size_t __sz> SDK_CP_ALWAYS_INLINE void enumerate_files(char const* dir_path, char const* (&exclude)[__sz], __func&& f) {
        SDK_CP_ASSERT(nullptr != dir_path);
        SDK_CP_ASSERT(0 != sdk_cp_strlen(dir_path));

        for (auto& entry : std::filesystem::directory_iterator(dir_path)) {
            auto const tmp     = entry.path().filename().generic_string();
            auto const fn_name = tmp.c_str();
            auto const fn_size = tmp.size();

            for (auto& ex : exclude) {
                if (0 == strcmp(ex, fn_name)) {
                    continue;
                }
                f(dir_path, fn_name, fn_size);
            }
        }
    }
#endif//_WIN32
}
