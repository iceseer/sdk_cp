///////////////////////////
// author: Alexander Lednev
// date: 28.08.2018
///////////////////////////
namespace algorithm {

    template<uint64_t NUM_PTS, uint64_t DISCRETE_COUNT, typename PT> class b_spline {
        enum { NUM_SEG = NUM_PTS - 3 };
        typedef point_2d<PT> point;

        struct point_info {
            point p;
            bool  inited;
            point_info() : inited(false) { }
        };
        struct segment_info {
            point p[DISCRETE_COUNT];
            bool  inited;
            segment_info() : inited(false) { }
        };
        typedef containers::static_frame<point_info,   NUM_PTS> point_container;
        typedef containers::static_frame<segment_info, NUM_SEG> segment_container;

        point_container   _point_container;
        segment_container _segment_container;

        static_assert(NUM_PTS > 4, "num point must be more than 4 to create spline");
        inline void _calculate(point const& p0, point const& p1, point const& p2, point const& p3, float(&a)[4], float(&b)[4]) {
            float* __restrict _a = a;
            float* __restrict _b = b;

            *_a++ = ( p0.x + 4*p1.x + p2.x) / 6;
            *_a++ = (-p0.x + p2.x ) / 2;
            *_a++ = ( p0.x - 2*p1.x + p2.x) / 2;
            *_a++ = (-p0.x + 3*p1.x - 3*p2.x + p3.x) / 6;

            *_b++ = ( p0.y + 4*p1.y + p2.y) / 6;
            *_b++ = (-p0.y + p2.y) / 2;
            *_b++ = ( p0.y - 2*p1.y + p2.y) / 2;
            *_b++ = (-p0.y + 3*p1.y - 3*p2.y + p3.y) / 6;
        }

    public:
        b_spline(b_spline const&)            = delete;
        b_spline& operator=(b_spline const&) = delete;
        b_spline()                           = default;

        void add_point(PT const& x, PT const& y) {
            auto& np = _point_container.move_fwd([&](point_info& p) { p.inited = false; });
            np.p.x = x; np.p.y = y;
            np.inited = true;

            auto const& p3 = _point_container.get_from_first(0);
            auto const& p2 = _point_container.get_from_first(1);
            auto const& p1 = _point_container.get_from_first(2);
            auto const& p0 = _point_container.get_from_first(3);

            if (!p3.inited || !p2.inited || !p1.inited || !p0.inited) { return; }

            float a[4]; float b[4];
            _calculate(p0.p, p1.p, p2.p, p3.p, a, b);

            auto& si = _segment_container.move_fwd([&](segment_info& s) { s.inited = false; });
            si.inited = true;

            point* __restrict p = si.p;
            for (uint64_t ix = 0; ix < DISCRETE_COUNT; ++ix) {
                float const t = float(ix) / DISCRETE_COUNT;
                p->x = (a[0] + t * (a[1] + t * (a[2] + t * a[3])));
                p->y = (b[0] + t * (b[1] + t * (b[2] + t * b[3])));
                ++p;
            }
        }

        bool get_point(PT const& x, PT& y) {
            for (uint64_t offset = 0; offset < NUM_SEG; ++offset) {
                auto& seg = _segment_container.get_from_first(offset);
                if (!seg.inited) { break; }

                if (x >= seg.p[0].x && x <= seg.p[DISCRETE_COUNT - 1].x) {
                    point const* const b = &seg.p[0];
                    point const* const e = &seg.p[DISCRETE_COUNT];
                    auto const* const it = std::lower_bound(b, e, x, [](point const& a, PT const& x) {
                        return a.x < x;
                    });

                    if (it != e) {
                        y = it->y;
                        return true;
                    }
                }
            }
            return false;
        }
    };

}
