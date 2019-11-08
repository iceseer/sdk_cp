static const char hex2[] =
                            "000102030405060708090a0b0c0d0e0f"
                            "101112131415161718191a1b1c1d1e1f"
                            "202122232425262728292a2b2c2d2e2f"
                            "303132333435363738393a3b3c3d3e3f"
                            "404142434445464748494a4b4c4d4e4f"
                            "505152535455565758595a5b5c5d5e5f"
                            "606162636465666768696a6b6c6d6e6f"
                            "707172737475767778797a7b7c7d7e7f"
                            "808182838485868788898a8b8c8d8e8f"
                            "909192939495969798999a9b9c9d9e9f"
                            "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
                            "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
                            "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
                            "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
                            "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
                            "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

template<typename __string_type/*, typename __allocator_type*/> class json_writer final {
    SDK_CP_INLINE static void toHex16Bit(unsigned int x, __string_type& out_str) {
        const unsigned int hi = (x >> 8) & 0xff;
        const unsigned int lo = x & 0xff;

        out_str += hex2[2 * hi];
        out_str += hex2[2 * hi + 1];
        out_str += hex2[2 * lo];
        out_str += hex2[2 * lo + 1];
    }

    SDK_CP_INLINE static uint32_t utf8ToCodepoint(const char*& s, const char* e) {
        const unsigned int REPLACEMENT_CHARACTER = 0xFFFD;

        unsigned int firstByte = static_cast<unsigned char>(*s);

        if (firstByte < 0x80)
            return firstByte;

        if (firstByte < 0xE0) {
            if (e - s < 2)
                return REPLACEMENT_CHARACTER;

            unsigned int calculated = ((firstByte & 0x1F) << 6)
                | (static_cast<unsigned int>(s[1]) & 0x3F);
            s += 1;
            // oversized encoded characters are invalid
            return calculated < 0x80 ? REPLACEMENT_CHARACTER : calculated;
        }

        if (firstByte < 0xF0) {
            if (e - s < 3)
                return REPLACEMENT_CHARACTER;

            unsigned int calculated = ((firstByte & 0x0F) << 12)
                | ((static_cast<unsigned int>(s[1]) & 0x3F) << 6)
                | (static_cast<unsigned int>(s[2]) & 0x3F);
            s += 2;
            // surrogates aren't valid codepoints itself
            // shouldn't be UTF-8 encoded
            if (calculated >= 0xD800 && calculated <= 0xDFFF)
                return REPLACEMENT_CHARACTER;
            // oversized encoded characters are invalid
            return calculated < 0x800 ? REPLACEMENT_CHARACTER : calculated;
        }

        if (firstByte < 0xF8) {
            if (e - s < 4)
                return REPLACEMENT_CHARACTER;

            unsigned int calculated = ((firstByte & 0x07) << 18)
                | ((static_cast<unsigned int>(s[1]) & 0x3F) << 12)
                | ((static_cast<unsigned int>(s[2]) & 0x3F) << 6)
                | (static_cast<unsigned int>(s[3]) & 0x3F);
            s += 3;
            // oversized encoded characters are invalid
            return calculated < 0x10000 ? REPLACEMENT_CHARACTER : calculated;
        }

        return REPLACEMENT_CHARACTER;
    }

    SDK_CP_INLINE static void value_2_quoted_str(const char* value, uint32_t length, __string_type& out_str) {
        if (value == nullptr) return;

        __string_type::size_type const maxsize = length * 2 + 3; // allescaped+quotes+NULL
        out_str.reserve(out_str.size() + maxsize); // to avoid lots of mallocs
        out_str += "\"";

        char const* end = value + length;
        for (const char* c = value; c != end; ++c) {
            switch (*c) {
            case '\"':
                out_str += "\\\"";
                break;
            case '\\':
                out_str += "\\\\";
                break;
            case '\b':
                out_str += "\\b";
                break;
            case '\f':
                out_str += "\\f";
                break;
            case '\n':
                out_str += "\\n";
                break;
            case '\r':
                out_str += "\\r";
                break;
            case '\t':
                out_str += "\\t";
                break;
                // case '/':
                // Even though \/ is considered a legal escape in JSON, a bare
                // slash is also legal, so I see no reason to escape it.
                // (I hope I am not misunderstanding something.)
                // blep notes: actually escaping \/ may be useful in javascript to avoid </
                // sequence.
                // Should add a flag to allow this compatibility mode and prevent this
                // sequence from occurring.
            default: {
                        unsigned int cp = utf8ToCodepoint(c, end);
                        // don't escape non-control characters
                        // (short escape sequence are applied above)
                        if (cp < 0x80 && cp >= 0x20)
                            out_str += static_cast<char>(cp);
                        else if (cp < 0x10000) { // codepoint is in Basic Multilingual Plane
                            out_str += "\\u";
                            toHex16Bit(cp, out_str);
                        }
                        else { // codepoint is not in Basic Multilingual Plane
                               // convert to surrogate pair first
                            cp -= 0x10000;
                            out_str += "\\u";
                            toHex16Bit((cp >> 10) + 0xD800, out_str);
                            out_str += "\\u";
                            toHex16Bit((cp & 0x3FF) + 0xDC00, out_str);
                        }
                    }
                     break;
            }
        }
        out_str += "\"";
    }

    SDK_CP_ALWAYS_INLINE void _write_value(const Json::Value& value, __string_type& out) {
        switch (value.type()) {
        case Json::nullValue:
            out += "null";
            break;
        case Json::intValue:
            tools::str_conv(value.asLargestInt(), out);
            break;
        case Json::uintValue:
            tools::str_conv(value.asLargestUInt(), out);
            break;
        case Json::realValue:
            tools::str_conv(value.asDouble(), out);
            break;
        case Json::stringValue:
        {
            char const* str;
            char const* end;
            if (value.getString(&str, &end)) {
                value_2_quoted_str(str, static_cast<unsigned>(end - str), out);
            }
            break;
        }
        case Json::booleanValue:
            tools::str_conv(value.asBool(), out);
            break;
        case Json::arrayValue: {
            out += '[';
            Json::ArrayIndex size = value.size();
            for (Json::ArrayIndex index = 0; index < size; ++index) {
                if (index > 0)
                    out += ',';
                _write_value(value[index], out);
            }
            out += ']';
        } break;
        case Json::objectValue: {
            Json::Value::Members members(value.getMemberNames());
            out += '{';
            for (Json::Value::Members::iterator it = members.begin(); it != members.end();
                ++it) {
                auto const& name = *it;
                if (it != members.begin()) {
                    out += ',';
                }
                value_2_quoted_str(name.data(), static_cast<unsigned>(name.length()), out);
                out += ":";
                _write_value(value[name], out);
            }
            out += '}';
        } break;
        }
    }

public:
    json_writer() = default;

    json_writer(json_writer const&) = delete;
    json_writer(json_writer&&)      = delete;

    json_writer& operator=(json_writer const&)  = delete;
    json_writer& operator=(json_writer&&)       = delete;

    SDK_CP_INLINE void write(Json::Value& root, __string_type& out) {
        _write_value(root, out);
        out += "\n";
    }
};