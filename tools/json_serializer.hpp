inline bool _get_field(Json::Value const& json, char const* name, bool& result) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && value->isBool()) {
        result = value->asBool();
        return true;
    }
    return false;
}
inline bool _get_field(Json::Value const& json, char const* name, double& result) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && value->isDouble()) {
        result = value->asDouble();
        return true;
    }
    return false;
}
inline bool _get_field(Json::Value const& json, char const* name, uint16_t& result) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && value->isUInt()) {
        result = (uint16_t)value->asUInt();
        return true;
    }
    return false;
}
inline bool _get_field(Json::Value const& json, char const* name, uint32_t& result) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && value->isUInt()) {
        result = value->asUInt();
        return true;
    }
    return false;
}
inline bool _get_field(Json::Value const& json, char const* name, int32_t& result) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && value->isInt()) {
        result = value->asInt();
        return true;
    }
    return false;
}
inline bool _get_field(Json::Value const& json, char const* name, int64_t& result) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && value->isInt64()) {
        result = value->asInt64();
        return true;
    }
    return false;
}
inline bool _get_field(Json::Value const& json, char const* name, uint64_t& result) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && value->isUInt64()) {
        result = value->asUInt64();
        return true;
    }
    return false;
}
template<uint64_t __size> inline bool _get_field(Json::Value const& json, char const* name, char(&result)[__size]) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && value->isString()) {
        strncpy(result, value->asCString(), __size);
        return true;
    }
    return false;
}
template<typename __string_type> inline bool _get_field(Json::Value const& json, char const* name, __string_type& result) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    char const* begin, *end;
    if (nullptr != value && value->getString(&begin, &end)) {
        result.assign(begin, end);
        return true;
    }
    return false;
}
template<uint64_t __size> inline bool _get_field(Json::Value const& json, char const* name, wchar_t (&result)[__size]) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && value->isString()) {
        std::mbstowcs(result, value->asCString(), __size);
        return true;
    }
    return false;
}
inline bool _get_field(Json::Value const& json, char const* name, Json::Value const*& result) {
    SDK_CP_ASSERT(nullptr != name);
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && !value->isNull()) {
        result = value;
        return true;
    }
    return false;
}
template<uint64_t __size> inline bool _get_field(Json::Value const& json, char const* name, uint32_t(&result)[__size]) {
    SDK_CP_ASSERT(nullptr != name);

    // new fields set to the end of array
    auto value = json.find(name, name + strlen(name));
    if (nullptr != value && value->isArray()) {
        uint64_t ix = 0;
        for (auto it = value->begin(); it != value->end(); ++it) {
            if (!it->isUInt()) {
                tools::log::error("Unexpected field type. Expect ", ix, " fiels as UINT32. Data: ", value->toStyledString().c_str());
                return false;
            }
            result[ix++] = it->asUInt();
        }
        return true;
    }
    return false;
}
template<uint64_t __size> SDK_CP_INLINE bool add_to_json(wchar_t const (& src)[__size], Json::Value& dst) {
    std::string tmp;
    tools::str_conv(src, tmp);
    dst = std::move(tmp);
    return true;
}
template<typename __type> SDK_CP_INLINE bool add_to_json(std::vector<__type> const& src, Json::Value& dst) {
    for (size_t ix = 0; ix < src.size(); ++ix) {
        if (!add_to_json(src[ix], dst[(Json::Value::ArrayIndex)ix])) {
            return false;
        }
    }
    return true;
}
SDK_CP_INLINE bool add_to_json(wchar_t const* src, Json::Value& dst) {
    std::string tmp;
    tools::str_conv(src, tmp);
    dst = std::move(tmp);
    return true;
}
SDK_CP_INLINE bool add_to_json(std::wstring const& src, Json::Value& dst) {
    std::string tmp;
    tools::str_conv(src, tmp);
    dst = std::move(tmp);
    return true;
}
