namespace runtime {

    template<typename __storage> class api_module final {
        typedef std::basic_string<char, std::char_traits<char>, std::allocator<char>> str_t;

        inline HMODULE _load_library(char const* name) {
            return ::LoadLibraryA(name);
        }

        HMODULE _module; str_t  _module_name;
        inline bool _check_module() {
            if (nullptr != _module) {
                return true;
            }

            if (_module_name.empty()) {
                return false;
            }

            _module = _load_library(_module_name.c_str());
            return (nullptr != _module);
        }
        template<class T> inline bool _loadMethod(str_t const& name, T& _method) const
        {
            _method = (T) ::GetProcAddress(_module, name.c_str());
            return nullptr != _method;
        }

    public:
        api_module(api_module const&) = delete;
        api_module& operator=(api_module const&) = delete;

        api_module(str_t const& module_name) : _module(nullptr), _module_name(module_name) {

        }

        ~api_module() {
            if (nullptr != _module) {
                ::FreeLibrary(_module);
            }
        }

        template<typename __ret_type, typename...__arg_types> bool get_method(std::function<__ret_type(__arg_types...)>& out, str_t const& name) {
            typedef __ret_type(func_type) (__arg_types...);
            if (_check_module()) {
                func_type* func;
                if (_loadMethod(name, func)) {
                    out = func;
                    return true;
                }
            }
            return false;
        }

        __storage storage;
    };

}
