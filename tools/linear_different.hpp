///////////////////////////
// author: Alexander Lednev
// date: 28.08.2018
///////////////////////////
namespace algorithm {

    template<typename __approximation> class linear_different {
        __approximation _approximation;

    public:
        linear_different() = default;
        linear_different(linear_different const&) = delete;
        linear_different& operator=(linear_different const&) = delete;

        template<typename PT> void add_point(PT const& x, PT const& y) {
            _approximation.add_point(x, y);
        }

        template<typename PT> bool get_different(PT const& x1, PT const& x2, PT& diff) {
            bool result = false;
            do {
                PT y1, y2;

                if (x1 > x2) { break; }
                if (!_approximation.get_point(x1, y1)) { break; }
                if (!_approximation.get_point(x2, y2)) { break; }

                diff = y2 - y1;
                result = true;
            } while(false);
            return result;
        }

        template<typename PT> bool get_normalized_different(PT const& x1, PT const& x2, float& diff) {
            bool result = false;
            do {
                PT y1, y2;

                if (x1 > x2) { break; }
                if (!_approximation.get_point(x1, y1)) { break; }
                if (!_approximation.get_point(x2, y2)) { break; }

                float const yf1 = float(y1);
                float const yf2 = float(y2);

                diff = (y2 / y1) - 1.0f;
                result = true;
            } while(false);
            return result;
        }
    };

}