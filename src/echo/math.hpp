/*
    Copyright © 2018 Emilia "Endorfina" Majewska

    This file is part of Violet.

    Violet is free software: you can study it, redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    Violet is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Violet.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <type_traits>
#include <array>
#include <cmath>

#include <memory.h> // memcpy

#define	F_E		    2.7182818284590452354f	/* e */
#define	F_LOG2E		1.4426950408889634074f	/* log 2e */
#define	F_LOG10E	0.43429448190325182765f	/* log 10e */
#define	F_LN2		0.69314718055994530942f	/* log e2 */
#define	F_LN10		2.30258509299404568402f	/* log e10 */
#define	F_PI		3.14159265358979323846f	/* pi */
#define	F_PI_2		1.57079632679489661923f	/* pi/2 */
#define	F_PI_4		0.78539816339744830962f	/* pi/4 */
#define	F_1_PI		0.31830988618379067154f	/* 1/pi */
#define	F_2_PI		0.63661977236758134308f	/* 2/pi */
#define	F_2_SQRTPI	1.12837916709551257390f	/* 2/sqrt(pi) */
#define	F_SQRT2		1.41421356237309504880f	/* sqrt(2) */
#define	F_SQRT1_2	0.70710678118654752440f	/* 1/sqrt(2) */

#define	F_TAU		6.28318530717958647692f	/* tau */
#define	F_TAU_2		F_PI	/* tau/2 */
#define	F_TAU_4		F_PI_2	/* tau/4 */
#define	F_TAU_8		F_PI_4	/* tau/4 */

#ifndef M_PI
#define M_PI F_PI
#endif

namespace Violet
{
    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    constexpr bool is_power_of_2(const T a) {
        using int_t = std::conditional_t<sizeof(T) >= sizeof(int), T, std::conditional_t<std::is_signed_v<T>, signed int, unsigned int>>;
        for (int_t it = 1, max = static_cast<int_t>(a); it <= max && it > 0; it *= 2)
            if (it == max)
                return true;
        return false;
    }

    template<typename T>
    constexpr inline T sqr(T a) {
        return a * a;
    }

	template<typename T,
		typename = std::enable_if_t<std::is_scalar_v<T>>>
    struct point2
    {
        using value_type = T;
        value_type x, y;
        point2() = default;
        point2(value_type a, value_type b) : x(a), y(b) {}

        constexpr point2 &operator+=(const point2 & other) {
			x += other.x;
			y += other.y;
			return *this;
		}

		constexpr point2 & operator-=(const point2 & other) {
			x -= other.x;
			y -= other.y;
			return *this;
		}

        constexpr point2 &operator*=(const point2 & other) {
			x *= other.x;
			y *= other.y;
			return *this;
		}

		constexpr point2 & operator/=(const point2 & other) {
			x /= other.x;
			y /= other.y;
			return *this;
		}

		constexpr friend point2 operator*(point2 first, const point2 & second) { return first *= second; }

		constexpr friend point2 operator/(point2 first, const point2 & second) { return first /= second; }

		constexpr friend point2 operator+(point2 first, const point2 & second) { return first += second; }

		constexpr friend point2 operator-(point2 first, const point2 & second) { return first -= second; }

		constexpr friend point2 operator*(point2 first, value_type second) { return first *= second; }

		constexpr friend point2 operator/(point2 first, value_type second) { return first /= second; }

        constexpr point2& operator*=(value_type f) {
            x *= f;
            y *= f;
            return *this;
        }
        constexpr point2& operator/=(value_type f) {
            x /= f;
            y /= f;
            return *this;
        }

		constexpr inline value_type product(const point2 & other) { return x * other.x + y * other.y; }

		// Distance
		friend float operator^(const point2 & first, const point2 & second) {
			return sqrtf(sqr(first.x - second.x) + sqr(first.y - second.y));
		}
    };


    template<typename T>
    bool is_inside(const point2<T> (&rect)[4], const point2<T> &p) {
        point2<T> axis[2] {
                { rect[0].x - rect[1].x, rect[0].y - rect[1].y },
                { rect[0].x - rect[3].x, rect[0].y - rect[3].y }
        };
        T s, min_a, max_a;
        for (unsigned n = 0; n < 2; ++n) {
            for (unsigned i = 0; i < 4; ++i) {
                s = (((rect[i] * axis[n]) / (axis[n].x * axis[n].x + axis[n].y * axis[n].y))
                     * axis[n]) % axis[n];
                if (i == 0)
                    min_a = max_a = s;
                else if (s < min_a)
                    min_a = s;
                else if (s > max_a)
                    max_a = s;
            }
            s = (((p * axis[n]) / (axis[n].x * axis[n].x + axis[n].y * axis[n].y))
                 * axis[n]) % axis[n];
            if (s > max_a || s < min_a)
                return false;
        }
        return true;
    }

    template<typename T>
    bool collision(const point2<T> (&a)[4], const point2<T> (&b)[4])
    {
        point2<T> axis[4] {
                { a[0].x - a[1].x, a[0].y - a[1].y },
                { a[0].x - a[3].x, a[0].y - a[3].y },
                { b[0].x - b[1].x, b[0].y - b[1].y },
                { b[0].x - b[3].x, b[0].y - b[3].y }
        };
        T s, min_a, min_b, max_a, max_b;
        for (int n = 0, i; n < 4; ++n) {
            for (i = 0; i < 4; ++i) {
                s = (((a[i] * axis[n]) / (axis[n].x * axis[n].x + axis[n].y * axis[n].y))
                     * axis[n]) % axis[n];
                if (i == 0)
                    min_a = max_a = s;
                else if (s < min_a)
                    min_a = s;
                else if (s > max_a)
                    max_a = s;
            }
            for (i = 0; i < 4; ++i) {
                s = (((b[i] * axis[n]) / (axis[n].x * axis[n].x + axis[n].y * axis[n].y))
                     * axis[n]) % axis[n];
                if (i == 0)
                    min_b = max_b = s;
                else if (s < min_b)
                    min_b = s;
                else if (s > max_b)
                    max_b = s;
            }
            if (min_b > max_a || max_b < min_a)
                return false;
        }
        return true;
    }




    template<typename T,
		typename = std::enable_if_t<std::is_scalar_v<T>>>
    struct color
    {
        using value_type = T;
        value_type r = 0, g = 0, b = 0, a = 1;
        color() = default;
        color(value_type _r, value_type _g, value_type _b) : r(_r), g(_g), b(_b) {}
        color(value_type _r, value_type _g, value_type _b, value_type _a) : r(_r), g(_g), b(_b), a(_a) {}

        constexpr static color greyscale(value_type x, value_type a = 1) {
            return { x, x, x, a };
        }

        constexpr color& operator*=(const color &other) {
            r *= other.r;
            g *= other.g;
            b *= other.b;
            a *= other.a;
            return *this;
        }
        
		constexpr friend color operator*(color first, const color & second) { return first *= second; }
    };

    template<typename T>
    struct grid {
        typedef T value_type;

        value_type ** m = nullptr;

        const size_t width;
        const size_t height;

    private:
        inline void create_arrays() {
            m = new value_type*[width];
            *m = new value_type[width * height]; // data
            for (size_t i = 1; i < width; ++i)
                m[i] = *m + (i * height);
        }

    public:

        grid(size_t w, size_t h) : width(w), height(h) {
            create_arrays();
        }

        grid(const grid &_cp) : width(_cp.width), height(_cp.height) {
            create_arrays();
            memcpy(*m, *_cp.m, width * height * sizeof(value_type));
        }

        grid(grid &&_mv) noexcept : width(_mv.width), height(_mv.height) {
            m = _mv.m;
            _mv.m = nullptr;
        }

        ~grid(void) {
            if (m) {
                delete[](*m); // data
                delete[]m;
            }
        }

        inline value_type * operator [](size_t i) { return m[i]; }
    };


    template<typename T,
		typename = std::enable_if_t<std::is_scalar_v<T>>>
    struct matrix4x4
    {
        using value_type = T;
        value_type val[16];

        matrix4x4() = default;

        matrix4x4(const std::array<value_type, 16>& _m) noexcept { memcpy(val, _m.data(), 16 * sizeof(value_type)); }

        matrix4x4(value_type (&_m)[16]) { memcpy(val, _m, 16 * sizeof(value_type)); }

        matrix4x4(value_type * _m) { memcpy(val, _m, 16 * sizeof(value_type)); }


        constexpr inline operator const value_type *() const { return val; }

        constexpr inline value_type operator [](unsigned i) const { return val[i]; }

    private:
        constexpr inline static void __set_identity(value_type (&dest)[16]) {
            dest = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            //memcpy(dest, id, 16 * sizeof(float));
        }

        constexpr static void __multiplication(value_type (&dest)[16], const value_type (&first)[16], const value_type (&second)[16])
        {
            for (int i = 0; i < 16; ++i) {
                dest[i] = first[i - (i % 4)] * second[i % 4];
                for (int n = 1; n < 4; ++n)
                    dest[i] += first[i - (i % 4) + n] * second[(i % 4) + n * 4];
            }
        }

        constexpr static bool __gluInvertMatrix(const value_type (&_mat)[16], value_type (&invOut)[16])
        {
            value_type inv[16], det;
            int i = 0;

            inv[0] = _mat[5]  * _mat[10] * _mat[15] -
                    _mat[5]  * _mat[11] * _mat[14] -
                    _mat[9]  * _mat[6]  * _mat[15] +
                    _mat[9]  * _mat[7]  * _mat[14] +
                    _mat[13] * _mat[6]  * _mat[11] -
                    _mat[13] * _mat[7]  * _mat[10];

            inv[4] = -_mat[4]  * _mat[10] * _mat[15] +
                    _mat[4]  * _mat[11] * _mat[14] +
                    _mat[8]  * _mat[6]  * _mat[15] -
                    _mat[8]  * _mat[7]  * _mat[14] -
                    _mat[12] * _mat[6]  * _mat[11] +
                    _mat[12] * _mat[7]  * _mat[10];

            inv[8] = _mat[4]  * _mat[9] * _mat[15] -
                    _mat[4]  * _mat[11] * _mat[13] -
                    _mat[8]  * _mat[5] * _mat[15] +
                    _mat[8]  * _mat[7] * _mat[13] +
                    _mat[12] * _mat[5] * _mat[11] -
                    _mat[12] * _mat[7] * _mat[9];

            inv[12] = -_mat[4]  * _mat[9] * _mat[14] +
                    _mat[4]  * _mat[10] * _mat[13] +
                    _mat[8]  * _mat[5] * _mat[14] -
                    _mat[8]  * _mat[6] * _mat[13] -
                    _mat[12] * _mat[5] * _mat[10] +
                    _mat[12] * _mat[6] * _mat[9];

            inv[1] = -_mat[1]  * _mat[10] * _mat[15] +
                    _mat[1]  * _mat[11] * _mat[14] +
                    _mat[9]  * _mat[2] * _mat[15] -
                    _mat[9]  * _mat[3] * _mat[14] -
                    _mat[13] * _mat[2] * _mat[11] +
                    _mat[13] * _mat[3] * _mat[10];

            inv[5] = _mat[0]  * _mat[10] * _mat[15] -
                    _mat[0]  * _mat[11] * _mat[14] -
                    _mat[8]  * _mat[2] * _mat[15] +
                    _mat[8]  * _mat[3] * _mat[14] +
                    _mat[12] * _mat[2] * _mat[11] -
                    _mat[12] * _mat[3] * _mat[10];

            inv[9] = -_mat[0]  * _mat[9] * _mat[15] +
                    _mat[0]  * _mat[11] * _mat[13] +
                    _mat[8]  * _mat[1] * _mat[15] -
                    _mat[8]  * _mat[3] * _mat[13] -
                    _mat[12] * _mat[1] * _mat[11] +
                    _mat[12] * _mat[3] * _mat[9];

            inv[13] = _mat[0]  * _mat[9] * _mat[14] -
                    _mat[0]  * _mat[10] * _mat[13] -
                    _mat[8]  * _mat[1] * _mat[14] +
                    _mat[8]  * _mat[2] * _mat[13] +
                    _mat[12] * _mat[1] * _mat[10] -
                    _mat[12] * _mat[2] * _mat[9];

            inv[2] = _mat[1]  * _mat[6] * _mat[15] -
                    _mat[1]  * _mat[7] * _mat[14] -
                    _mat[5]  * _mat[2] * _mat[15] +
                    _mat[5]  * _mat[3] * _mat[14] +
                    _mat[13] * _mat[2] * _mat[7] -
                    _mat[13] * _mat[3] * _mat[6];

            inv[6] = -_mat[0]  * _mat[6] * _mat[15] +
                    _mat[0]  * _mat[7] * _mat[14] +
                    _mat[4]  * _mat[2] * _mat[15] -
                    _mat[4]  * _mat[3] * _mat[14] -
                    _mat[12] * _mat[2] * _mat[7] +
                    _mat[12] * _mat[3] * _mat[6];

            inv[10] = _mat[0]  * _mat[5] * _mat[15] -
                    _mat[0]  * _mat[7] * _mat[13] -
                    _mat[4]  * _mat[1] * _mat[15] +
                    _mat[4]  * _mat[3] * _mat[13] +
                    _mat[12] * _mat[1] * _mat[7] -
                    _mat[12] * _mat[3] * _mat[5];

            inv[14] = -_mat[0]  * _mat[5] * _mat[14] +
                    _mat[0]  * _mat[6] * _mat[13] +
                    _mat[4]  * _mat[1] * _mat[14] -
                    _mat[4]  * _mat[2] * _mat[13] -
                    _mat[12] * _mat[1] * _mat[6] +
                    _mat[12] * _mat[2] * _mat[5];

            inv[3] = -_mat[1] * _mat[6] * _mat[11] +
                    _mat[1] * _mat[7] * _mat[10] +
                    _mat[5] * _mat[2] * _mat[11] -
                    _mat[5] * _mat[3] * _mat[10] -
                    _mat[9] * _mat[2] * _mat[7] +
                    _mat[9] * _mat[3] * _mat[6];

            inv[7] = _mat[0] * _mat[6] * _mat[11] -
                    _mat[0] * _mat[7] * _mat[10] -
                    _mat[4] * _mat[2] * _mat[11] +
                    _mat[4] * _mat[3] * _mat[10] +
                    _mat[8] * _mat[2] * _mat[7] -
                    _mat[8] * _mat[3] * _mat[6];

            inv[11] = -_mat[0] * _mat[5] * _mat[11] +
                    _mat[0] * _mat[7] * _mat[9] +
                    _mat[4] * _mat[1] * _mat[11] -
                    _mat[4] * _mat[3] * _mat[9] -
                    _mat[8] * _mat[1] * _mat[7] +
                    _mat[8] * _mat[3] * _mat[5];

            inv[15] = _mat[0] * _mat[5] * _mat[10] -
                    _mat[0] * _mat[6] * _mat[9] -
                    _mat[4] * _mat[1] * _mat[10] +
                    _mat[4] * _mat[2] * _mat[9] +
                    _mat[8] * _mat[1] * _mat[6] -
                    _mat[8] * _mat[2] * _mat[5];

            det = _mat[0] * inv[0] + _mat[1] * inv[4] + _mat[2] * inv[8] + _mat[3] * inv[12];

            if (det == 0)
                return false;

            det = 1.0 / det;

            for (i = 0; i < 16; ++i)
                invOut[i] = inv[i] * det;

            return true;
        }

    public:

        constexpr matrix4x4& operator=(const matrix4x4& other) {
            memcpy(val, other.val, 16 * sizeof(value_type));
            return *this;
        }
        constexpr matrix4x4& operator*=(const matrix4x4& other){
            value_type _copy[16];
            memcpy(_copy, val, 16 * sizeof(value_type));
            __multiplication(val, _copy, other.val);
            return *this;
        }
        friend constexpr matrix4x4 operator*(matrix4x4 lhs, const matrix4x4& rhs) {
            return lhs *= rhs;
        }



        constexpr inline matrix4x4& invert(void) {
            __gluInvertMatrix(val, val);
            return *this;
        }

        constexpr static matrix4x4 identity() {
            matrix4x4 _mat;
            __set_identity(_mat.val);
            return _mat;
        }
        constexpr static matrix4x4 translate(value_type x, value_type y) {
            matrix4x4 _mat;
            __set_identity(_mat.val);
            _mat.val[12] = x;
            _mat.val[13] = y;
            return _mat;
        }
        //constexpr static matrix4x4 translate(const std::pair<value_type, value_type> &p);
        constexpr static matrix4x4 translate(const point2<value_type> &p) {
            matrix4x4 _mat;
            __set_identity(_mat.val);
            _mat.val[12] = p.x;
            _mat.val[13] = p.y;
            return _mat;
        }
        constexpr static matrix4x4 scale(value_type w, value_type h, value_type x = 0, value_type y = 0) {
            matrix4x4 _mat;
            __set_identity(_mat.val);
            _mat.val[0] = w;
            _mat.val[5] = h;
            _mat.val[12] = -x * w + x;
            _mat.val[13] = -y * h + y;
            return _mat;
        }
        constexpr static matrix4x4 scale(value_type w, value_type h, const point2<value_type> &p) {
            matrix4x4 _mat;
            __set_identity(_mat.val);
            _mat.val[0] = w;
            _mat.val[5] = h;
            _mat.val[12] = -p.x * w + p.x;
            _mat.val[13] = -p.y * h + p.y;
            return _mat;
        }
        constexpr static inline matrix4x4 scale(value_type sc) {
            matrix4x4 _mat;
            __set_identity(_mat.val);
            _mat.val[0] = sc;
            _mat.val[5] = sc;
            return _mat;
        }
        constexpr static matrix4x4 rotate_deg(value_type angle, value_type x = 0, value_type y = 0) {
            matrix4x4 _mat;
            __set_identity(_mat.val);
            const auto r = degtorad(angle);
            _mat.val[5] = _mat.val[0] = cosf(r);
            _mat.val[4] = -(_mat.val[1] = sinf(r));

            _mat.val[12] = -x * _mat.val[0] + -y * _mat.val[4] + x;
            _mat.val[13] = -x * _mat.val[1] + -y * _mat.val[5] + y;
            return _mat;
        }
        constexpr static matrix4x4 rotate_deg(value_type angle, const point2<value_type> &p) {
            matrix4x4 _mat;
            __set_identity(_mat.val);
            const auto r = degtorad(angle);
            _mat.val[5] = _mat.val[0] = cosf(r);
            _mat.val[4] = -(_mat.val[1] = sinf(r));

            _mat.val[12] = -p.x * _mat.val[0] + -p.y * _mat.val[4] + p.x;
            _mat.val[13] = -p.x * _mat.val[1] + -p.y * _mat.val[5] + p.y;
            return _mat;
        }
        constexpr static matrix4x4 rotate(value_type rad, value_type x = 0, value_type y = 0) {
            matrix4x4 _mat;
            __set_identity(_mat.val);
            _mat.val[5] = _mat.val[0] = cosf(rad);
            _mat.val[4] = -(_mat.val[1] = sinf(rad));

            _mat.val[12] = -x * _mat.val[0] + -y * _mat.val[4] + x;
            _mat.val[13] = -x * _mat.val[1] + -y * _mat.val[5] + y;
            return _mat;
        }
        constexpr static matrix4x4 rotate(value_type rad, const point2<value_type> &p) {
            matrix4x4 _mat;
            __set_identity(_mat.val);
            _mat.val[5] = _mat.val[0] = cosf(rad);
            _mat.val[4] = -(_mat.val[1] = sinf(rad));

            _mat.val[12] = -p.x * _mat.val[0] + -p.y * _mat.val[4] + p.x;
            _mat.val[13] = -p.x * _mat.val[1] + -p.y * _mat.val[5] + p.y;
            return _mat;
        }

        constexpr static matrix4x4 orthof(value_type l, value_type r, value_type t, value_type b, value_type n, value_type f) {
            value_type mat[16]{ 0 };
            if (r != l && t != b && f != n) {
                mat[0] = 2 / (r - l);
                mat[5] = 2 / (t - b);
                mat[10] = -2 / (f - n);
                mat[15] = 1.f;
                mat[14] = -((f + n) / (f - n));
                mat[13] = -((t + b) / (t - b));
                mat[12] = -((r + l) / (r - l));
            }
            return mat;
        }
    };

    template<typename T,
		typename = std::enable_if_t<std::is_scalar_v<T>>>
    struct matrix4x4_opt : public matrix4x4<T>
    {
        using value_type = typename matrix4x4<T>::value_type;

    private:
        constexpr static void __optimised_multiplication(value_type (&dest)[16], const value_type (&first)[16], const value_type (&second)[16])
        {
            memset(dest, 0, 16 * sizeof(float));
            dest[0] = first[0] * second[0] + first[1] * second[4];
            dest[1] = first[0] * second[1] + first[1] * second[5];
            dest[4] = first[4] * second[0] + first[5] * second[4];
            dest[5] = first[4] * second[1] + first[5] * second[5];
            dest[10] = 1;
            dest[12] = first[12] * second[0] + first[13] * second[4] + second[12];
            dest[13] = first[12] * second[1] + first[13] * second[5] + second[13];
            dest[15] = 1;
        }

    public:
        constexpr matrix4x4_opt& operator*=(const matrix4x4<value_type>& other){
            value_type _copy[16];
            memcpy(_copy, this->val, 16 * sizeof(value_type));
            __optimised_multiplication(this->val, _copy, other.val);
            return *this;
        }
        constexpr matrix4x4_opt& operator*=(const matrix4x4_opt& other){
            value_type _copy[16];
            memcpy(_copy, this->val, 16 * sizeof(value_type));
            __optimised_multiplication(this->val, _copy, other.val);
            return *this;
        }
        friend constexpr matrix4x4_opt operator*(matrix4x4_opt lhs, const matrix4x4<value_type>& rhs) {
            return lhs *= rhs;
        }
        friend constexpr matrix4x4_opt operator*(matrix4x4<value_type> lhs, const matrix4x4_opt& rhs) {
            return lhs *= rhs;
        }
        friend constexpr matrix4x4_opt operator*(matrix4x4_opt lhs, const matrix4x4_opt& rhs) {
            return lhs *= rhs;
        }
    };
}