SDK_CP_INLINE void handleErrors(void) {
    while (auto errCode = ERR_get_error()) {
        tools::log::error("[OpenSSL error] ", ERR_error_string(errCode, NULL));
    }
}

static int decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,
    unsigned char *iv, unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        handleErrors();
        return 0;
    }

    /*
     * Initialise the decryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) {
        handleErrors();
        return 0;
    }

    /*
     * Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary.
     */
    if (1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len)) {
        handleErrors();
        return 0;
    }
    plaintext_len = len;

    /*
     * Finalise the decryption. Further plaintext bytes may be written at
     * this stage.
     */
    if (1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len)) {
        handleErrors();
        return 0;
    }
    plaintext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len;
}

static int encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key,
    unsigned char *iv, unsigned char *ciphertext)
{
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        handleErrors();
        return -1;
    }

    /*
     * Initialise the encryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)) {
        handleErrors();
        return -1;
    }

    /*
     * Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
        handleErrors();
        return -1;
    }
    ciphertext_len = len;

    /*
     * Finalise the encryption. Further ciphertext bytes may be written at
     * this stage.
     */
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
        handleErrors();
        return -1;
    }
    ciphertext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

static int32_t gcm_encrypt(unsigned char *plaintext, int plaintext_len,
    unsigned char *aad, int aad_len,
    unsigned char *key,
    unsigned char *iv, int iv_len,
    unsigned char *ciphertext,
    unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx; 
    int len; 
    int ciphertext_len;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        handleErrors();
        return 0;
    }

    /* Initialise the encryption operation. */
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        handleErrors();
        return 0;
    }

    /*
     * Set IV length if default 12 bytes (96 bits) is not appropriate
     */
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL)) {
        handleErrors();
        return 0;
    }

    /* Initialise key and IV */
    if (1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)) {
        handleErrors();
        return 0;
    }

    /*
     * Provide any AAD data. This can be called zero or more times as
     * required
     */
    if (1 != EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len)) {
        handleErrors();
        return 0;
    }

    /*
     * Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
        handleErrors();
        return 0;
    }
    ciphertext_len = len;

    /*
     * Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
        handleErrors();
        return 0;
    }
    ciphertext_len += len;

    /* Get the tag */
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) {
        handleErrors();
        return 0;
    }

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

static int gcm_decrypt(unsigned char *ciphertext, int ciphertext_len,
    unsigned char *aad, int aad_len,
    unsigned char *tag,
    unsigned char *key,
    unsigned char *iv, int iv_len,
    unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    int ret;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        handleErrors();
        return 0;
    }

    /* Initialise the decryption operation. */
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) {
        handleErrors();
        return 0;
    }

    /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL)) {
        handleErrors();
        return 0;
    }

    /* Initialise key and IV */
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)) {
        handleErrors();
        return 0;
    }

    /*
     * Provide any AAD data. This can be called zero or more times as
     * required
     */
    if (!EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len)) {
        handleErrors();
        return 0;
    }

    /*
     * Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if (!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len)) {
        handleErrors();
        return 0;
    }
    plaintext_len = len;

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag)) {
        handleErrors();
        return 0;
    }

    /*
     * Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
    ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    if (ret > 0) {
        /* Success */
        plaintext_len += len;
        return plaintext_len;
    }
    else {
        /* Verify failed */
        return -1;
    }
}

struct user_t final {
private:
    tools::threading::spin_blocker _wscon_cs;
    ws_conn_t*                     _wscon;
    std::string                    _msg;
    std::string                    _pong_data;
    evutil_socket_t                _clisockfd;

    uint8_t*                       _aes_key;
    uint8_t*                       _aes_ivc;
    bool const                     _encrypt;

    template<typename...__args>  SDK_CP_ALWAYS_INLINE void _trace(__args const&...args) {
        tools::log::trace("[WS]",
            "\n   user connection id: ", (uint64_t)_clisockfd,
            "\n   IN data size: ",       _msg.size(),
            "\n   msg: ",                args...
        );
    }
    template<typename...__args>  SDK_CP_ALWAYS_INLINE void _error(__args const&...args) {
        tools::log::error("[WS]",
            "\n   user connection id: ", (uint64_t)_clisockfd,
            "\n   IN data size: ",       _msg.size(),
            "\n   msg: ",                args...
        );
    }
    SDK_CP_ALWAYS_INLINE void _on_pong_received() {
        SDK_CP_ASSERT(nullptr != _wscon);
        if (_wscon->frame->payload_len > 0) {
            _pong_data.append(_wscon->frame->payload_data, _wscon->frame->payload_len);
        }
        if (_wscon->frame->fin == 1) {
            _trace("Received pong ", _pong_data.size(), " bytes.");
            if (!!on_pong_recv) {
                on_pong_recv(this);
            }
            _pong_data.clear();
        }
    }
    SDK_CP_ALWAYS_INLINE void _on_frame_received() {
        SDK_CP_ASSERT(nullptr != _wscon);
        if (_wscon->frame->payload_len > 0) {
            _msg.append(_wscon->frame->payload_data, _wscon->frame->payload_len);
        }

        if (_wscon->frame->fin == 1) {
            _trace("Received frame ", _msg.size(), " bytes.");
            if (!!on_frame_recv) {
                on_frame_recv(this);
            }
            _msg.clear();
        }
    }
    static void _pong_recv_cb(void* arg) {
        SDK_CP_ASSERT(nullptr != arg);
        auto user = (user_t*)arg;
        user->_on_pong_received();
    }
    static void _frame_recv_cb(void* arg) {
        SDK_CP_ASSERT(nullptr != arg);
        auto user = (user_t*)arg;
        user->_on_frame_received();
    }

    SDK_CP_ALWAYS_INLINE void _on_user_disconnect() {
        _trace("Disconnected.");
        if(!!on_close) {
            on_close(this);
        }
        if (!!on_disconnect) {
            on_disconnect(this);
        }
    }
    static void _user_disconnect_cb(void *arg) {
        if (auto user = (user_t*)arg) {
            user->_on_user_disconnect();
        }
    }

public:
    tools::callback_unit<void(user_t*)> on_pong_recv ;
    tools::callback_unit<void(user_t*)> on_frame_recv;
    tools::callback_unit<void(user_t*)> on_disconnect;
    tools::callback_unit<void(user_t*)> on_close     ;

public:
    user_t(user_t const&) = delete;
    user_t(user_t&&)      = delete;

    user_t& operator=(user_t const&) = delete;
    user_t& operator=(user_t&&)      = delete;

    explicit user_t(evutil_socket_t clisockfd, bool const encrypt)
        : _clisockfd(clisockfd)
        , _wscon(ws_conn_new()) 
        , _aes_key(nullptr)
        , _aes_ivc(nullptr)
        , _encrypt(encrypt)
    {
    }
    ~user_t() {
        if (nullptr != _wscon) {
            ws_conn_free(_wscon);
        }
    }

    void store_aes_data(uint8_t aes_key[32], uint8_t aes_ivc[16]) {
        _aes_key = aes_key;
        _aes_ivc = aes_ivc;
        SDK_CP_ASSERT((nullptr != _aes_key && nullptr != _aes_ivc) || (nullptr == _aes_key && nullptr == _aes_ivc));
    }

    uint8_t* get_aes_key() {
        return _aes_key;
    }

    uint8_t* get_aes_ivc() {
        return _aes_ivc;
    }

    bool is_encrypt() {
        return _encrypt;
    }

public:
    void extract_data(std::string& data) {
        data.swap(_msg);
        _msg.clear();
    }
    void start_handling() {
        if (nullptr == _wscon) {
            _error("WSconnection not created.");
            return;
        }

        _trace("Begin handling events");
        ws_conn_setcb (_wscon, FRAME_RECV, _frame_recv_cb,      this);
        ws_conn_setcb(_wscon,  PONG,       _pong_recv_cb,       this);
        ws_conn_setcb (_wscon, CLOSE,      _user_disconnect_cb, this);
        ws_serve_start(_wscon);
    }

    static void fb_begin_prepare(std::string& data) {
        data.assign(frame_buffer_t::k_prefix_size, '\0');
    }

    static frame_buffer_t fb_end_prepare_ping(std::string& data) {
        return frame_buffer_new(1, 0x9, data);
    }

    static frame_buffer_t fb_end_prepare(bool const enc, uint8_t* aes_key, uint8_t* aes_ivc, std::string& data, std::string& tmp) {
        SDK_CP_ASSERT((nullptr != aes_key && nullptr != aes_ivc) || (nullptr == aes_key && nullptr == aes_ivc));
        if (enc && nullptr != aes_key && nullptr != aes_ivc) {
            tmp.resize(data.size() + EVP_MAX_BLOCK_LENGTH);
            memcpy(&tmp[0], &data[0], frame_buffer_t::k_prefix_size);

            auto const len = encrypt(
                (uint8_t*)&data[frame_buffer_t::k_prefix_size],
                (int32_t)data.size() - frame_buffer_t::k_prefix_size,
                aes_key, aes_ivc,
                (uint8_t*)&tmp[frame_buffer_t::k_prefix_size]);

            if (-1 == len) {
                tools::log::error("Unable to encrypt data.");
                return frame_buffer_new(1, 1, data);
            }

            //std::string d2;
            //d2.resize(tmp.size());
            //memcpy(&d2[0], &tmp[0], frame_buffer_t::k_prefix_size);

            //auto const len2 = decrypt(
            //    (uint8_t*)&tmp[frame_buffer_t::k_prefix_size], len,
            //    aes_key, aes_ivc,
            //    (uint8_t*)&d2[frame_buffer_t::k_prefix_size]
            //);
            tmp.resize(len + frame_buffer_t::k_prefix_size);
            return frame_buffer_new(1, 1, tmp);
        }
        return frame_buffer_new(1, 1, data);
    }

    //void prepare(std::string& data) {
    //    data.assign(frame_buffer_t::k_prefix_size, '\0');
    //}

    bool send(frame_buffer_t& evb) {
        tools::threading::spin_blocker_infinite guard(_wscon_cs);
        if (nullptr == _wscon) {
            _error("WSconnection not created.");
            return false;
        }
        //uint8_t key[] = "01234567890123456789012345678901";
        //uint8_t iv[]  = "0123456789012345";

        //uint8_t ciphertext[2048];
        //uint8_t tag[16];
        //gcm_encrypt((uint8_t*)evb.data, evb.len, (uint8_t*)"12345", 5, key, iv, 16, ciphertext, tag);

        if (send_a_frame(_wscon, &evb) != 0) {
            _error("Send frame failed.");
            return false;
        }

        //_trace("Sent successfully ", evb->len, " butes.");
        return true;
    }

    //bool send(std::string& data) {
    //    if (data.empty()) {
    //        _error("'data' is empty. Nothing to send.");
    //        return false;
    //    }

    //    //TODO: переписать framebuffer и сделать внутри вектор, который свопать с внешними данными
    //    auto buffer_deleter = [](frame_buffer_t* obj) { if (nullptr != obj) frame_buffer_free(obj); };
    //    std::unique_ptr<frame_buffer_t, decltype(buffer_deleter)> evb(frame_buffer_new(1, 1, data.size(), data.c_str()), buffer_deleter);

    //    if (!evb) {
    //        _error("Frame buffer creation failed.");
    //        return false;
    //    }

    //    tools::threading::spin_blocker_infinite guard(_wscon_cs);
    //    if (nullptr == _wscon) {
    //        _error("WSconnection not created.");
    //        return false;
    //    }

    //    if (send_a_frame(_wscon, evb.get()) != 0) {
    //        _error("Send frame failed.");
    //        return false;
    //    }

    //    //_trace("Sent successfully ", evb->len, " butes.");
    //    return true;
    //}
    void disconnect() {
        tools::threading::spin_blocker_infinite guard(_wscon_cs);
        if (nullptr == _wscon) {
            _error("WSconnection not created.");
            return;
        }

        if (nullptr != _wscon->bev) {
            bufferevent_free(_wscon->bev);
            _wscon->bev = nullptr;
        } else {
            //SDK_CP_ASSERT(false);
        }
    }
    void attach_buffer_eventsock(bufferevent* bev) {
        SDK_CP_ASSERT(nullptr != bev);

        tools::threading::spin_blocker_infinite guard(_wscon_cs);
        if (nullptr == _wscon) {
            _error("WSconnection not created.");
            return;
        }

        SDK_CP_ASSERT(_wscon->bev == nullptr);
        _wscon->bev = bev;
    }
};
