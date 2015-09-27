#if ! defined(_SG14_FIXED_POINT)
#define _SG14_FIXED_POINT 1

#include <climits>
#include <cinttypes>
#include <type_traits>

#if defined(__clang__) || defined(__GNUG__)
// sg14::float_point only fully supports 64-bit types with the help of 128-bit ints.
// Clang and GCC use (__int128) and (unsigned __int128) for 128-bit ints.
#define _SG14_FIXED_POINT_128
#endif

namespace sg14
{
	////////////////////////////////////////////////////////////////////////////////
	// general-purpose _impl definitions

	namespace _impl
	{
		////////////////////////////////////////////////////////////////////////////////
		// num_bits

		template <typename T>
		constexpr int num_bits()
		{
			return sizeof(T) * CHAR_BIT;
		}

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::make_int

		template <bool Signed, int NumBytes>
		struct _make_int;

		// specializations
		template <> struct _make_int<false, 1> { using type = std::uint8_t; };
		template <> struct _make_int<true, 1> { using type = std::int8_t; };
		template <> struct _make_int<false, 2> { using type = std::uint16_t; };
		template <> struct _make_int<true, 2> { using type = std::int16_t; };
		template <> struct _make_int<false, 4> { using type = std::uint32_t; };
		template <> struct _make_int<true, 4> { using type = std::int32_t; };
		template <> struct _make_int<false, 8> { using type = std::uint64_t; };
		template <> struct _make_int<true, 8> { using type = std::int64_t; };
#if defined(_SG14_FIXED_POINT_128)
		template <> struct _make_int<false, 16> { using type = unsigned __int128; };
		template <> struct _make_int<true, 16> { using type = __int128; };
#endif

		template <bool Signed, int NumBytes>
		using make_int = typename _make_int<Signed, NumBytes>::type;

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::make_float

		template <int NumBytes>
		struct _make_float;

		// specializations
		template <> struct _make_float<1> { using type = float; };
		template <> struct _make_float<2> { using type = float; };
		template <> struct _make_float<4> { using type = float; };
		template <> struct _make_float<8> { using type = double; };
		template <> struct _make_float<16> { using type = long double; };

		template <int NumBytes>
		using make_float = typename _make_float<NumBytes>::type;

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::is_integral

		template <class T>
		struct is_integral;

		// exception to std::is_integral as fixed_point::operator bool is a special case
		template <>
		struct is_integral<bool> : std::false_type { };

#if defined(_SG14_FIXED_POINT_128)
		// addresses https://llvm.org/bugs/show_bug.cgi?id=23156
		template <>
		struct is_integral<__int128> : std::true_type { };

		template <>
		struct is_integral<unsigned __int128> : std::true_type { };
#endif

		template <class T>
		struct is_integral : std::is_integral<T> { };

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::is_signed

		template <class T>
		struct is_signed
		{
			static_assert(is_integral<T>::value, "sg14::_impl::is_signed only intended for use with integral types");
			static constexpr bool value = std::is_same<make_int<true, sizeof(T)>, T>::value;
		};

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::is_unsigned

		template <class T>
		struct is_unsigned
		{
			static_assert(is_integral<T>::value, "sg14::_impl::is_unsigned only intended for use with integral types");
			static constexpr bool value = std::is_same<make_int<false, sizeof(T)>, T>::value;
		};

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::make_signed

		template <class T>
		struct _make_signed
		{
			using type = make_int<true, sizeof(T)>;
		};

		template <class T>
		using make_signed = typename _make_signed<T>::type;

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::make_unsigned

		template <class T>
		struct _make_unsigned
		{
			using type = make_int<false, sizeof(T)>;
		};

		template <class T>
		using make_unsigned = typename _make_unsigned<T>::type;

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::next_size

		// given an integral type, IntType,
		// provides the integral type of the equivalent type with twice the size
		template <class IntType>
		using next_size = make_int<_impl::is_signed<IntType>::value, sizeof(IntType) * 2>;

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::previous_size

		// given an integral type, IntType,
		// provides the integral type of the equivalent type with half the size
		template <class IntType>
		using previous_size = make_int<_impl::is_signed<IntType>::value, sizeof(IntType) / 2>;

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::shift_left and sg14::_impl::shift_right

		// performs a shift operation by a fixed number of bits avoiding two pitfalls:
		// 1) shifting by a negative amount causes undefined behavior
		// 2) converting between integer types of different sizes can lose significant bits during shift right

		// Exponent == 0
		template <
			int Exponent,
			class Output,
			class Input,
			typename std::enable_if<
				(Exponent == 0),
				int>::type Dummy = 0>
		constexpr Output shift_left(Input i) noexcept
		{
			static_assert(_impl::is_integral<Input>::value, "Input must be integral type");
			static_assert(_impl::is_integral<Output>::value, "Output must be integral type");

			// cast only
			return static_cast<Output>(i);
		}

		template <
			int Exponent,
			class Output,
			class Input,
			typename std::enable_if<
				Exponent == 0,
				int>::type Dummy = 0>
		constexpr Output shift_right(Input i) noexcept
		{
			static_assert(_impl::is_integral<Input>::value, "Input must be integral type");
			static_assert(_impl::is_integral<Output>::value, "Output must be integral type");

			// cast only
			return static_cast<Output>(i);
		}

		// sizeof(Input) > sizeof(Output)
		template <
			int Exponent,
			class Output,
			class Input,
			typename std::enable_if<
				!(Exponent <= 0) && sizeof(Output) <= sizeof(Input) && _impl::is_unsigned<Input>::value,
				int>::type Dummy = 0>
		constexpr Output shift_left(Input i) noexcept
		{
			return shift_left<0, Output, Input>(i) << Exponent;
		}

		template <
			int Exponent,
			class Output,
			class Input,
			typename std::enable_if<
				!(Exponent <= 0) && sizeof(Output) <= sizeof(Input),
				int>::type Dummy = 0>
		constexpr Output shift_right(Input i) noexcept
		{
			return shift_right<0, Output, Input>(i >> Exponent);
		}

		// sizeof(Input) <= sizeof(Output)
		template <
			int Exponent, 
			class Output, 
			class Input, 
			typename std::enable_if<
				!(Exponent <= 0) && !(sizeof(Output) <= sizeof(Input)) && _impl::is_unsigned<Input>::value,
				char>::type Dummy = 0>
		constexpr Output shift_left(Input i) noexcept
		{
			return shift_left<0, Output, Input>(i) << Exponent;
		}

		template <
			int Exponent,
			class Output,
			class Input,
			typename std::enable_if<
				!(Exponent <= 0) && !(sizeof(Output) <= sizeof(Input)),
				char>::type Dummy = 0>
		constexpr Output shift_right(Input i) noexcept
		{
			return shift_right<0, Output, Input>(i) >> Exponent;
		}

		// is_signed<Input>
		template <
			int Exponent,
			class Output,
			class Input,
			typename std::enable_if<
				!(Exponent <= 0) && _impl::is_signed<Input>::value,
				int>::type Dummy = 0>
		constexpr Output shift_left(Input i) noexcept
		{
			using unsigned_input = _impl::make_unsigned<Input>;
			using signed_output = _impl::make_signed<Output>;

			return (i >= 0)
				? shift_left<Exponent, signed_output, unsigned_input>(i)
				: -shift_left<Exponent, signed_output, unsigned_input>(-i);
		}

		// Exponent < 0
		template <
			int Exponent,
			class Output,
			class Input,
			typename std::enable_if<
				(Exponent < 0),
				int>::type Dummy = 0>
		constexpr Output shift_left(Input i) noexcept
		{
			// negate Exponent and flip from left to right
			return shift_right<-Exponent, Output, Input>(i);
		}

		template <
			int Exponent,
			class Output,
			class Input,
			typename std::enable_if<
				Exponent < 0,
				int>::type Dummy = 0>
		constexpr Output shift_right(Input i) noexcept
		{
			// negate Exponent and flip from right to left
			return shift_left<-Exponent, Output, Input>(i);
		}

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::pow2

		// returns given power of 2
		template <class S, int Exponent, typename std::enable_if<Exponent == 0, int>::type Dummy = 0>
		constexpr S pow2() noexcept
		{
			static_assert(std::is_floating_point<S>::value, "S must be floating-point type");
			return 1;
		}

		template <class S, int Exponent, typename std::enable_if<!(Exponent <= 0), int>::type Dummy = 0>
		constexpr S pow2() noexcept
		{
			static_assert(std::is_floating_point<S>::value, "S must be floating-point type");
			return pow2<S, Exponent - 1>() * S(2);
		}

		template <class S, int Exponent, typename std::enable_if<!(Exponent >= 0), int>::type Dummy = 0>
		constexpr S pow2() noexcept
		{
			static_assert(std::is_floating_point<S>::value, "S must be floating-point type");
			return pow2<S, Exponent + 1>() * S(.5);
		}

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::max

		template <class T>
		constexpr const T& max( const T& a, const T& b )
		{
			return (a < b) ? b : a;
		}

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::common_repr_type

		// given two or more integral types, produces a common type with enough capacity
		// to store values of either EXCEPT when one is signed and both are same size
		template <class ... ReprTypes>
		struct _common_repr_type;

		template <class ReprTypeHead>
		struct _common_repr_type <ReprTypeHead>
		{
			using type = ReprTypeHead;
		};

		template <class ReprTypeHead, class ... ReprTypeTail>
		struct _common_repr_type <ReprTypeHead, ReprTypeTail...>
		{
			using _tail_type = typename _common_repr_type<ReprTypeTail...>::type;

			using type = _impl::make_int<
				_impl::is_signed<ReprTypeHead>::value | _impl::is_signed<_tail_type>::value,
				_impl::max(sizeof(ReprTypeHead), sizeof(_tail_type))>;
		};

		template <class ... ReprTypes>
		using common_repr_type = typename _common_repr_type<ReprTypes...>::type;

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::capacity

		// has value that, given a value N, 
		// returns number of bits necessary to represent it in binary
		template <unsigned N> struct capacity;

		template <>
		struct capacity<0>
		{
			static constexpr int value = 0;
		};

		template <unsigned N>
		struct capacity
		{
			static constexpr int value = capacity<N / 2>::value + 1;
		};

		////////////////////////////////////////////////////////////////////////////////
		// _impl::sufficient_repr

		// given a required number of bits a type should have and whether it is signed,
		// provides a built-in integral type with necessary capacity
		template <unsigned RequiredBits, bool IsSigned>
		using sufficient_repr 
			= make_int<IsSigned, 1 << (capacity<((RequiredBits + 7) / 8) - 1>::value)>;

		////////////////////////////////////////////////////////////////////////////////
		// sg14::sqrt helper functions

		template <class ReprType>
		constexpr ReprType sqrt_bit(
			ReprType n,
			ReprType bit = ReprType(1) << (num_bits<ReprType>() - 2)) noexcept
		{
			return (bit > n) ? sqrt_bit<ReprType>(n, bit >> 2) : bit;
		}

		template <class ReprType>
		constexpr ReprType sqrt_solve3(
			ReprType n,
			ReprType bit,
			ReprType result) noexcept
		{
			return bit
				   ? (n >= result + bit)
					 ? sqrt_solve3<ReprType>(n - (result + bit), bit >> 2, (result >> 1) + bit)
					 : sqrt_solve3<ReprType>(n, bit >> 2, result >> 1)
				   : result;
		}

		template <class ReprType>
		constexpr ReprType sqrt_solve1(ReprType n) noexcept
		{
			return sqrt_solve3<ReprType>(n, sqrt_bit<ReprType>(n), 0);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::fixed_point class template definition
	//
	// approximates a real number using a built-in integral type;
	// somewhat like a floating-point number but with exponent determined at run-time

	template <class ReprType = int, int Exponent = 0>
	class fixed_point
	{
	public:
		////////////////////////////////////////////////////////////////////////////////
		// types

		using repr_type = ReprType;

		////////////////////////////////////////////////////////////////////////////////
		// constants

		constexpr static int exponent = Exponent;
		constexpr static int digits = _impl::num_bits<ReprType>() - _impl::is_signed<repr_type>::value;
		constexpr static int integer_digits = digits + exponent;
		constexpr static int fractional_digits = digits - integer_digits;

		////////////////////////////////////////////////////////////////////////////////
		// functions

	private:
		// constructor taking representation explicitly using operator++(int)-style trick
		constexpr fixed_point(repr_type repr, int) noexcept
			: _repr(repr)
		{
		}
	public:
		// default c'tor
		fixed_point() noexcept {}

		// c'tor taking an integer type
		template <class S, typename std::enable_if<_impl::is_integral<S>::value, int>::type Dummy = 0>
		explicit constexpr fixed_point(S s) noexcept
			: _repr(integral_to_repr(s))
		{
		}

		// c'tor taking a floating-point type
		template <class S, typename std::enable_if<std::is_floating_point<S>::value, int>::type Dummy = 0>
		explicit constexpr fixed_point(S s) noexcept
			: _repr(floating_point_to_repr(s))
		{
		}

		// c'tor taking a fixed-point type
		template <class FromReprType, int FromExponent>
		explicit constexpr fixed_point(const fixed_point<FromReprType, FromExponent> & rhs) noexcept
			: _repr(fixed_point_to_repr(rhs))
		{
		}

		// copy assignment operator taking an integer type
		template <class S, typename std::enable_if<_impl::is_integral<S>::value, int>::type Dummy = 0>
		fixed_point & operator=(S s) noexcept
		{
			_repr = integral_to_repr(s);
			return *this;
		}

		// copy assignment operator taking a floating-point type
		template <class S, typename std::enable_if<std::is_floating_point<S>::value, int>::type Dummy = 0>
		fixed_point & operator=(S s) noexcept
		{
			_repr = floating_point_to_repr(s);
			return *this;
		}

		// copy assignement operator taking a fixed-point type
		template <class FromReprType, int FromExponent>
		fixed_point & operator=(const fixed_point<FromReprType, FromExponent> & rhs) noexcept
		{
			_repr = fixed_point_to_repr(rhs);
			return *this;
		}

		// returns value represented as a floating-point
		template <class S, typename std::enable_if<_impl::is_integral<S>::value, int>::type Dummy = 0>
		explicit constexpr operator S() const noexcept
		{
			return repr_to_integral<S>(_repr);
		}

		// returns value represented as integral
		template <class S, typename std::enable_if<std::is_floating_point<S>::value, int>::type Dummy = 0>
		explicit constexpr operator S() const noexcept
		{
			return repr_to_floating_point<S>(_repr);
		}

		// returns non-zeroness represented as boolean
		explicit constexpr operator bool() const noexcept
		{
			return _repr != 0;
		}

		template <class Rhs, typename std::enable_if<std::is_arithmetic<Rhs>::value, int>::type Dummy = 0>
		fixed_point &operator*=(const Rhs & rhs) noexcept;

		template <class Rhs, typename std::enable_if<std::is_arithmetic<Rhs>::value, int>::type Dummy = 0>
		fixed_point & operator/=(const Rhs & rhs) noexcept;

		// returns internal representation of value
		constexpr repr_type data() const noexcept
		{
			return _repr;
		}

		// creates an instance given the underlying representation value
		static constexpr fixed_point from_data(repr_type repr) noexcept
		{
			return fixed_point(repr, 0);
		}

	private:
		template <class S, typename std::enable_if<std::is_floating_point<S>::value, int>::type Dummy = 0>
		static constexpr S one() noexcept
		{
			return _impl::pow2<S, - exponent>();
		}

		template <class S, typename std::enable_if<_impl::is_integral<S>::value, int>::type Dummy = 0>
		static constexpr S one() noexcept
		{
			return integral_to_repr<S>(1);
		}

		template <class S>
		static constexpr S inverse_one() noexcept
		{
			static_assert(std::is_floating_point<S>::value, "S must be floating-point type");
			return _impl::pow2<S, exponent>();
		}

		template <class S>
		static constexpr repr_type integral_to_repr(S s) noexcept
		{
			static_assert(_impl::is_integral<S>::value, "S must be unsigned integral type");

			return _impl::shift_right<exponent, repr_type>(s);
		}

		template <class S>
		static constexpr S repr_to_integral(repr_type r) noexcept
		{
			static_assert(_impl::is_integral<S>::value, "S must be unsigned integral type");

			return _impl::shift_left<exponent, S>(r);
		}

		template <class S>
		static constexpr repr_type floating_point_to_repr(S s) noexcept
		{
			static_assert(std::is_floating_point<S>::value, "S must be floating-point type");
			return static_cast<repr_type>(s * one<S>());
		}

		template <class S>
		static constexpr S repr_to_floating_point(repr_type r) noexcept
		{
			static_assert(std::is_floating_point<S>::value, "S must be floating-point type");
			return S(r) * inverse_one<S>();
		}

		template <class FromReprType, int FromExponent>
		static constexpr repr_type fixed_point_to_repr(const fixed_point<FromReprType, FromExponent> & rhs) noexcept
		{
			return _impl::shift_right<(exponent - FromExponent), repr_type>(rhs.data());
		}

		////////////////////////////////////////////////////////////////////////////////
		// variables

		repr_type _repr;
	};

	////////////////////////////////////////////////////////////////////////////////
	// sg14::make_fixed

	// given the desired number of integer and fractional digits,
	// generates a fixed_point type such that:
	//   fixed_point<>::integer_digits == IntegerDigits,
	// and
	//   fixed_point<>::fractional_digits >= FractionalDigits,
	template <unsigned IntegerDigits, unsigned FractionalDigits = 0, bool IsSigned = true>
	using make_fixed = fixed_point<
		_impl::sufficient_repr<IntegerDigits + FractionalDigits + IsSigned, IsSigned>,
		(signed)(IntegerDigits + IsSigned) - _impl::num_bits<_impl::sufficient_repr<IntegerDigits + FractionalDigits + IsSigned, IsSigned>>()>;

	////////////////////////////////////////////////////////////////////////////////
	// sg14::make_ufixed

	// unsigned short-hanrd for make_fixed
	template <unsigned IntegerDigits, unsigned FractionalDigits = 0>
	using make_ufixed = make_fixed<IntegerDigits, FractionalDigits, false>;

	////////////////////////////////////////////////////////////////////////////////
	// sg14::make_fixed_from_repr

	// yields a float_point with Exponent calculated such that
	// fixed_point<ReprType, Exponent>::integer_bits == IntegerDigits
	template <class ReprType, int IntegerDigits>
	using make_fixed_from_repr = fixed_point<
		ReprType,
		IntegerDigits + _impl::is_signed<ReprType>::value - (signed)sizeof(ReprType) * CHAR_BIT>;

	////////////////////////////////////////////////////////////////////////////////
	// sg14::promote_result / promote

	// given template parameters of a fixed_point specialization, 
	// yields alternative specialization with twice the fractional bits
	// and twice the integral/sign bits
	template <class FixedPoint>
	using promote_result = fixed_point<
		_impl::next_size<typename FixedPoint::repr_type>,
		FixedPoint::exponent * 2>;

	// as promote_result but promotes parameter, from
	template <class FixedPoint>
	promote_result<FixedPoint>
	constexpr promote(const FixedPoint & from) noexcept
	{
		return promote_result<FixedPoint>(from);
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::demote_result / demote

	// given template parameters of a fixed_point specialization, 
	// yields alternative specialization with half the fractional bits
	// and half the integral/sign bits (assuming Exponent is even)
	template <class FixedPoint>
	using demote_result = fixed_point<
		_impl::previous_size<typename FixedPoint::repr_type>,
		FixedPoint::exponent / 2>;

	// as demote_result but demotes parameter, from
	template <class FixedPoint>
	demote_result<FixedPoint>
	constexpr demote(const FixedPoint & from) noexcept
	{
		return demote_result<FixedPoint>(from);
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::fixed_point-aware _impl definitions

	namespace _impl
	{
		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::is_fixed_point

		template <class T>
		struct is_fixed_point;

		template <class T>
		struct is_fixed_point
			: public std::integral_constant<bool, false> {};

		template <class ReprType, int Exponent>
		struct is_fixed_point <fixed_point<ReprType, Exponent>>
			: public std::integral_constant<bool, true> {};

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::_common_type

		template <class Lhs, class Rhs, class _Enable = void>
		struct _common_type;

		// given two fixed-point, produces the type that is best suited to both of them
		template <class LhsReprType, int LhsExponent, class RhsReprType, int RhsExponent>
		struct _common_type<fixed_point<LhsReprType, LhsExponent>, fixed_point<RhsReprType, RhsExponent>>
		{
			using type = make_fixed_from_repr<
				_impl::common_repr_type<LhsReprType, RhsReprType>,
				_impl::max<int>(
					fixed_point<LhsReprType, LhsExponent>::integer_digits,
					fixed_point<RhsReprType, RhsExponent>::integer_digits)>;
		};

		// given a fixed-point and a integer type, 
		// generates a fixed-point type that is as big as both of them (or as close as possible)
		template <class LhsReprType, int LhsExponent, class RhsInteger>
		struct _common_type<
			fixed_point<LhsReprType, LhsExponent>,
			RhsInteger,
			typename std::enable_if<_impl::is_integral<RhsInteger>::value>::type>
		{
			using type = fixed_point<LhsReprType, LhsExponent>;
		};

		// given a fixed-point and a floating-point type, 
		// generates a floating-point type that is as big as both of them (or as close as possible)
		template <class LhsReprType, int LhsExponent, class Float>
		struct _common_type<
			fixed_point<LhsReprType, LhsExponent>,
			Float,
			typename std::enable_if<std::is_floating_point<Float>::value>::type>
		: std::common_type<_impl::make_float<sizeof(LhsReprType)>, Float>
		{
		};

		// when first type is not fixed-point and second type is, reverse the order
		template <class Lhs, class RhsReprType, int RhsExponent>
		struct _common_type<Lhs, fixed_point<RhsReprType, RhsExponent>>
		: _common_type<fixed_point<RhsReprType, RhsExponent>, Lhs>
		{
		};

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::common_type

		// similar to std::common_type 
		// but one or both input types must be fixed_point
		template <class Lhs, class Rhs>
		using common_type = typename _common_type<Lhs, Rhs>::type;

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::multiply

		template <class Result, class Lhs, class Rhs>
		constexpr Result multiply(const Lhs & lhs, const Rhs & rhs) noexcept
		{
			using result_repr_type = typename Result::repr_type;
			using intermediate_repr_type = _impl::next_size<typename common_type<Lhs, Rhs>::repr_type>;
			return Result::from_data(
				_impl::shift_left<(Lhs::exponent + Rhs::exponent - Result::exponent), result_repr_type>(
					static_cast<intermediate_repr_type>(lhs.data()) * static_cast<intermediate_repr_type>(rhs.data())));
		}

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::divide

		template <class FixedPointQuotient, class FixedPointDividend, class FixedPointDivisor>
		constexpr FixedPointQuotient divide(const FixedPointDividend & lhs, const FixedPointDivisor & rhs) noexcept
		{
			using result_repr_type = typename FixedPointQuotient::repr_type;

			// a fixed-point type which is capable of holding the value passed in to lhs
			// and the result of the lhs / rhs; depending greately on the exponent of each
			using intermediate_type = make_fixed<
				_impl::max(FixedPointQuotient::integer_digits, FixedPointDividend::integer_digits + FixedPointDivisor::fractional_digits),
				_impl::max(FixedPointQuotient::fractional_digits, FixedPointDividend::fractional_digits + FixedPointDivisor::integer_digits),
				_impl::is_signed<typename FixedPointQuotient::repr_type>::value
				|| _impl::is_signed<typename FixedPointDividend::repr_type>::value>;

			return FixedPointQuotient::from_data(
				_impl::shift_left<
					(intermediate_type::exponent - FixedPointDivisor::exponent - FixedPointQuotient::exponent),
					result_repr_type>
					(static_cast<intermediate_type>(lhs).data() / rhs.data()));
		}

		////////////////////////////////////////////////////////////////////////////////
		// sg14::_impl::add

		template <class Result, class FixedPoint, class Head>
		constexpr Result add(const Head & addend_head)
		{
			static_assert(std::is_same<FixedPoint, Head>::value, "mismatched add parameters");
			return static_cast<Result>(addend_head);
		}

		template <class Result, class FixedPoint, class Head, class ... Tail>
		constexpr Result add(const Head & addend_head, const Tail & ... addend_tail)
		{
			static_assert(std::is_same<FixedPoint, Head>::value, "mismatched add parameters");
			return add<Result, FixedPoint, Tail ...>(addend_tail ...) + static_cast<Result>(addend_head);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	// homogeneous (mixed-mode) operator overloads
	//
	// taking one or two identical fixed_point specializations

	template <class ReprType, int Exponent>
	constexpr bool operator==(
		const fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return lhs.data() == rhs.data();
	}

	template <class ReprType, int Exponent>
	constexpr bool operator!=(
		const fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return lhs.data() != rhs.data();
	}

	template <class ReprType, int Exponent>
	constexpr bool operator<(
		const fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return lhs.data() < rhs.data();
	}

	template <class ReprType, int Exponent>
	constexpr bool operator>(
		const fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return lhs.data() > rhs.data();
	}

	template <class ReprType, int Exponent>
	constexpr bool operator>=(
		const fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return lhs.data() >= rhs.data();
	}

	template <class ReprType, int Exponent>
	constexpr bool operator<=(
		const fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return lhs.data() <= rhs.data();
	}

	// arithmetic
	template <class ReprType, int Exponent>
	constexpr fixed_point<ReprType, Exponent> operator-(
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		static_assert(_impl::is_signed<ReprType>::value, "unary negation of unsigned value");

		return fixed_point<ReprType, Exponent>::from_data(-rhs.data());
	}

	template <class ReprType, int Exponent>
	constexpr fixed_point<ReprType, Exponent> operator+(
		const fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return fixed_point<ReprType, Exponent>::from_data(lhs.data() + rhs.data());
	}

	template <class ReprType, int Exponent>
	constexpr fixed_point<ReprType, Exponent> operator-(
		const fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return fixed_point<ReprType, Exponent>::from_data(lhs.data() - rhs.data());
	}

	template <class ReprType, int Exponent>
	fixed_point<ReprType, Exponent> & operator+=(
		fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return lhs = lhs + rhs;
	}

	template <class ReprType, int Exponent>
	fixed_point<ReprType, Exponent> & operator-=(
		fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return lhs = lhs - rhs;
	}

	template <class ReprType, int Exponent>
	fixed_point<ReprType, Exponent> & operator*=(
		fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return lhs = lhs * rhs;
	}

	template <class ReprType, int Exponent>
	fixed_point<ReprType, Exponent> & operator/=(
		fixed_point<ReprType, Exponent> & lhs,
		const fixed_point<ReprType, Exponent> & rhs) noexcept
	{
		return lhs = lhs / rhs;
	}

	////////////////////////////////////////////////////////////////////////////////
	// heterogeneous operator overloads
	//
	// compare two objects of different fixed_point specializations

	template <class Lhs, class Rhs>
	constexpr auto operator==(const Lhs & lhs, const Rhs & rhs) noexcept
	-> typename std::enable_if<_impl::is_fixed_point<Lhs>::value || _impl::is_fixed_point<Rhs>::value, bool>::type
	{
		using common_type = _impl::common_type<Lhs, Rhs>;
		return static_cast<common_type>(lhs) == static_cast<common_type>(rhs);
	}

	template <class Lhs, class Rhs>
	constexpr auto operator!=(const Lhs & lhs, const Rhs & rhs) noexcept
	-> typename std::enable_if<_impl::is_fixed_point<Lhs>::value || _impl::is_fixed_point<Rhs>::value, bool>::type
	{
		using common_type = _impl::common_type<Lhs, Rhs>;
		return static_cast<common_type>(lhs) != static_cast<common_type>(rhs);
	}

	template <class Lhs, class Rhs>
	constexpr auto operator<(const Lhs & lhs, const Rhs & rhs) noexcept
	-> typename std::enable_if<_impl::is_fixed_point<Lhs>::value || _impl::is_fixed_point<Rhs>::value, bool>::type
	{
		using common_type = _impl::common_type<Lhs, Rhs>;
		return static_cast<common_type>(lhs) < static_cast<common_type>(rhs);
	}

	template <class Lhs, class Rhs>
	constexpr auto operator>(const Lhs & lhs, const Rhs & rhs) noexcept
	-> typename std::enable_if<_impl::is_fixed_point<Lhs>::value || _impl::is_fixed_point<Rhs>::value, bool>::type
	{
		using common_type = _impl::common_type<Lhs, Rhs>;
		return static_cast<common_type>(lhs) > static_cast<common_type>(rhs);
	}

	template <class Lhs, class Rhs>
	constexpr auto operator>=(const Lhs & lhs, const Rhs & rhs) noexcept
	-> typename std::enable_if<_impl::is_fixed_point<Lhs>::value || _impl::is_fixed_point<Rhs>::value, bool>::type
	{
		using common_type = _impl::common_type<Lhs, Rhs>;
		return static_cast<common_type>(lhs) >= static_cast<common_type>(rhs);
	}

	template <class Lhs, class Rhs>
	constexpr auto operator<=(const Lhs & lhs, const Rhs & rhs) noexcept
	-> typename std::enable_if<_impl::is_fixed_point<Lhs>::value || _impl::is_fixed_point<Rhs>::value, bool>::type
	{
		using common_type = _impl::common_type<Lhs, Rhs>;
		return static_cast<common_type>(lhs) <= static_cast<common_type>(rhs);
	}

	////////////////////////////////////////////////////////////////////////////////
	// arithmetic
	
	template <class Lhs, class Rhs>
	constexpr auto operator+(
		const Lhs & lhs,
		const Rhs & rhs) noexcept
	-> _impl::common_type<Lhs, Rhs>
	{
		using common_type = _impl::common_type<Lhs, Rhs>;
		return static_cast<common_type>(lhs) + static_cast<common_type>(rhs);
	}

	template <class Lhs, class Rhs>
	constexpr auto operator-(
		const Lhs & lhs,
		const Rhs & rhs) noexcept
	-> _impl::common_type<Lhs, Rhs>
	{
		using common_type = _impl::common_type<Lhs, Rhs>;
		return static_cast<common_type>(lhs) - static_cast<common_type>(rhs);
	}

	// fixed-point, fixed-point -> fixed-point
	template <class LhsReprType, int LhsExponent, class RhsReprType, int RhsExponent>
	constexpr auto operator*(
		const fixed_point<LhsReprType, LhsExponent> & lhs,
		const fixed_point<RhsReprType, RhsExponent> & rhs) noexcept
	-> _impl::common_type<fixed_point<LhsReprType, LhsExponent>, fixed_point<RhsReprType, RhsExponent>>
	{
		using result_type = _impl::common_type<fixed_point<LhsReprType, LhsExponent>, fixed_point<RhsReprType, RhsExponent>>;
		return _impl::multiply<result_type>(lhs, rhs);
	}

	template <class LhsReprType, int LhsExponent, class RhsReprType, int RhsExponent>
	constexpr auto operator/(
		const fixed_point<LhsReprType, LhsExponent> & lhs,
		const fixed_point<RhsReprType, RhsExponent> & rhs) noexcept
	-> _impl::common_type<fixed_point<LhsReprType, LhsExponent>, fixed_point<RhsReprType, RhsExponent>>
	{
		using result_type = _impl::common_type<fixed_point<LhsReprType, LhsExponent>, fixed_point<RhsReprType, RhsExponent>>;
		return _impl::divide<result_type>(lhs, rhs);
	}

	// fixed-point, integer -> fixed-point
	template <class LhsReprType, int LhsExponent, class Integer>
	constexpr auto operator*(
		const fixed_point<LhsReprType, LhsExponent> & lhs,
		const Integer & rhs) noexcept
	-> typename std::enable_if<std::is_integral<Integer>::value, fixed_point<LhsReprType, LhsExponent>>::type
	{
		using result_type = fixed_point<LhsReprType, LhsExponent>;
		return _impl::multiply<result_type>(lhs, fixed_point<Integer>(rhs));
	}

	template <class LhsReprType, int LhsExponent, class Integer>
	constexpr auto operator/(
		const fixed_point<LhsReprType, LhsExponent> & lhs,
		const Integer & rhs) noexcept
	-> typename std::enable_if<std::is_integral<Integer>::value, fixed_point<LhsReprType, LhsExponent>>::type
	{
		using result_type = fixed_point<LhsReprType, LhsExponent>;
		return _impl::divide<result_type>(lhs, fixed_point<Integer>(rhs));
	}

	// integer. fixed-point -> fixed-point
	template <class Integer, class RhsReprType, int RhsExponent>
	constexpr auto operator*(
		const Integer & lhs,
		const fixed_point<RhsReprType, RhsExponent> & rhs) noexcept
	-> typename std::enable_if<std::is_integral<Integer>::value, fixed_point<RhsReprType, RhsExponent>>::type
	{
		using result_type = fixed_point<RhsReprType, RhsExponent>;
		return _impl::multiply<result_type>(fixed_point<Integer>(lhs), rhs);
	}

	template <class Integer, class RhsReprType, int RhsExponent>
	constexpr auto operator/(
		const Integer & lhs,
		const fixed_point<RhsReprType, RhsExponent> & rhs) noexcept
	-> typename std::enable_if<std::is_integral<Integer>::value, fixed_point<RhsReprType, RhsExponent>>::type
	{
		using result_type = fixed_point<RhsReprType, RhsExponent>;
		return _impl::divide<result_type>(fixed_point<Integer>(lhs), rhs);
	}

	// fixed-point, floating-point -> floating-point
	template <class LhsReprType, int LhsExponent, class Float>
	constexpr auto operator*(
		const fixed_point<LhsReprType, LhsExponent> & lhs,
		const Float & rhs) noexcept
	-> _impl::common_type<
		fixed_point<LhsReprType, LhsExponent>,
		typename std::enable_if<std::is_floating_point<Float>::value, Float>::type>
	{
		using result_type = _impl::common_type<fixed_point<LhsReprType, LhsExponent>, Float>;
		return static_cast<result_type>(lhs) * rhs;
	}

	template <class LhsReprType, int LhsExponent, class Float>
	constexpr auto operator/(
		const fixed_point<LhsReprType, LhsExponent> & lhs,
		const Float & rhs) noexcept
	-> _impl::common_type<
		fixed_point<LhsReprType, LhsExponent>,
		typename std::enable_if<std::is_floating_point<Float>::value, Float>::type>
	{
		using result_type = _impl::common_type<fixed_point<LhsReprType, LhsExponent>, Float>;
		return static_cast<result_type>(lhs) / rhs;
	}

	// floating-point, fixed-point -> floating-point
	template <class Float, class RhsReprType, int RhsExponent>
	constexpr auto operator*(
		const Float & lhs,
		const fixed_point<RhsReprType, RhsExponent> & rhs) noexcept
	-> _impl::common_type<
		typename std::enable_if<std::is_floating_point<Float>::value, Float>::type,
		fixed_point<RhsReprType, RhsExponent>>
	{
		using result_type = _impl::common_type<fixed_point<RhsReprType, RhsExponent>, Float>;
		return lhs * static_cast<result_type>(rhs);
	}

	template <class Float, class RhsReprType, int RhsExponent>
	constexpr auto operator/(
		const Float & lhs,
		const fixed_point<RhsReprType, RhsExponent> & rhs) noexcept
	-> _impl::common_type<
		typename std::enable_if<std::is_floating_point<Float>::value, Float>::type,
		fixed_point<RhsReprType, RhsExponent>>
	{
		using result_type = _impl::common_type<fixed_point<RhsReprType, RhsExponent>, Float>;
		return lhs /
			static_cast<result_type>(rhs);
	}

	template <class LhsReprType, int Exponent, class Rhs>
	fixed_point<LhsReprType, Exponent> & operator+=(fixed_point<LhsReprType, Exponent> & lhs, const Rhs & rhs) noexcept
	{
		return lhs += fixed_point<LhsReprType, Exponent>(rhs);
	}

	template <class LhsReprType, int Exponent, class Rhs>
	fixed_point<LhsReprType, Exponent> & operator-=(fixed_point<LhsReprType, Exponent> & lhs, const Rhs & rhs) noexcept
	{
		return lhs -= fixed_point<LhsReprType, Exponent>(rhs);
	}

	template <class LhsReprType, int Exponent>
	template <class Rhs, typename std::enable_if<std::is_arithmetic<Rhs>::value, int>::type Dummy>
	fixed_point<LhsReprType, Exponent> &
	fixed_point<LhsReprType, Exponent>::operator*=(const Rhs & rhs) noexcept
	{
		_repr *= rhs;
		return * this;
	}

	template <class LhsReprType, int Exponent>
	template <class Rhs, typename std::enable_if<std::is_arithmetic<Rhs>::value, int>::type Dummy>
	fixed_point<LhsReprType, Exponent> &
	fixed_point<LhsReprType, Exponent>::operator/=(const Rhs & rhs) noexcept
	{
		_repr /= rhs;
		return * this;
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::sqrt

	// https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Binary_numeral_system_.28base_2.29
	// placeholder implementation; slow when calculated at run-time?
	template <class ReprType, int Exponent>
	constexpr fixed_point<ReprType, Exponent>
	sqrt(const fixed_point<ReprType, Exponent> & x) noexcept
	{
		return fixed_point<ReprType, Exponent>::from_data(
			static_cast<ReprType>(_impl::sqrt_solve1(promote(x).data())));
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::trunc_add_result / trunc_add

	// yields specialization of fixed_point with integral bits necessary to store
	// result of an addition between N values of fixed_point<ReprType, Exponent>
	template <class FixedPoint, unsigned N = 2>
	using trunc_add_result = make_fixed_from_repr<
		typename FixedPoint::repr_type,
		fixed_point<
			typename FixedPoint::repr_type,
			FixedPoint::exponent>::integer_digits + _impl::capacity<N - 1>::value>;

	template <class FixedPoint, class ... Tail>
	trunc_add_result<FixedPoint, sizeof...(Tail) + 1>
	constexpr trunc_add(const FixedPoint & addend1, const Tail & ... addend_tail)
	{
		using output_type = trunc_add_result<FixedPoint, sizeof...(Tail) + 1>;
		return _impl::add<output_type, FixedPoint>(addend1, addend_tail ...);
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::trunc_subtract_result / trunc_subtract

	// yields specialization of fixed_point with integral bits necessary to store
	// result of an subtraction between N values of fixed_point<ReprType, Exponent>
	template <class Lhs, class Rhs = Lhs>
	using trunc_subtract_result = make_fixed_from_repr<
		_impl::make_int<true, _impl::max(sizeof(typename Lhs::repr_type), sizeof(typename Rhs::repr_type))>,
		_impl::max(Lhs::integer_digits, Rhs::integer_digits) + 1>;

	template <class Lhs, class Rhs>
	trunc_subtract_result<Lhs, Rhs>
	constexpr trunc_subtract(const Lhs & minuend, const Rhs & subtrahend)
	{
		using output_type = trunc_subtract_result<Lhs, Rhs>;
		return static_cast<output_type>(minuend) - static_cast<output_type>(subtrahend);
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::trunc_multiply_result / trunc_multiply

	// yields specialization of fixed_point with integral bits necessary to store
	// result of a multiply between values of fixed_point<ReprType, Exponent>
	template <class Lhs, class Rhs = Lhs>
	using trunc_multiply_result = make_fixed_from_repr<
		_impl::common_repr_type<typename Lhs::repr_type, typename Rhs::repr_type>,
		Lhs::integer_digits + Rhs::integer_digits>;

	// as trunc_multiply_result but converts parameter, factor,
	// ready for safe binary multiply
	template <class Lhs, class Rhs>
	trunc_multiply_result<Lhs, Rhs>
	constexpr trunc_multiply(const Lhs & lhs, const Rhs & rhs) noexcept
	{
		using result_type = trunc_multiply_result<Lhs, Rhs>;
		return _impl::multiply<result_type>(lhs, rhs);
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::trunc_divide_result / trunc_divide

	// yields specialization of fixed_point with integral bits necessary to store
	// result of a divide between values of types, Lhs and Rhs
	template <class FixedPointDividend, class FixedPointDivisor = FixedPointDividend>
	using trunc_divide_result = make_fixed_from_repr<
		_impl::common_repr_type<typename FixedPointDividend::repr_type, typename FixedPointDivisor::repr_type>,
		FixedPointDividend::integer_digits + FixedPointDivisor::fractional_digits>;

	// as trunc_divide_result but converts parameter, factor,
	// ready for safe binary divide
	template <class FixedPointDividend, class FixedPointDivisor>
	trunc_divide_result<FixedPointDividend, FixedPointDivisor>
	constexpr trunc_divide(const FixedPointDividend & lhs, const FixedPointDivisor & rhs) noexcept
	{
		using result_type = trunc_divide_result<FixedPointDividend, FixedPointDivisor>;
		return _impl::divide<result_type>(lhs, rhs);
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::trunc_reciprocal_result / trunc_reciprocal

	// yields specialization of fixed_point with integral bits necessary to store
	// result of inverse of value of type FixedPoint
	template <class FixedPoint>
	using trunc_reciprocal_result = make_fixed_from_repr<
		typename FixedPoint::repr_type,
		FixedPoint::fractional_digits + 1>;

	// returns reciprocal of fixed_point in same-sized fixed-point type
	// that can comfortably store significant digits of result
	template <class FixedPoint>
	trunc_reciprocal_result<FixedPoint>
	constexpr trunc_reciprocal(const FixedPoint & fixed_point) noexcept
	{
		using result_type = trunc_reciprocal_result<FixedPoint>;
		using result_repr_type = typename result_type::repr_type;

		using dividend_type = make_fixed_from_repr<result_repr_type, 1>;

		return _impl::divide<result_type>(dividend_type(1), fixed_point);
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::trunc_square_result / trunc_square

	// yields specialization of fixed_point with integral bits necessary to store
	// result of a multiply between values of fixed_point<ReprType, Exponent>
	// whose sign bit is set to the same value
	template <class FixedPoint>
	using trunc_square_result = make_fixed_from_repr<
		_impl::make_unsigned<typename FixedPoint::repr_type>,
		FixedPoint::integer_digits * 2>;

	// as trunc_square_result but converts parameter, factor,
	// ready for safe binary multiply-by-self
	template <class FixedPoint>
	trunc_square_result<FixedPoint>
	constexpr trunc_square(const FixedPoint & root) noexcept
	{
		using result_type = trunc_square_result<FixedPoint>;
		return _impl::multiply<result_type>(root, root);
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::trunc_sqrt_result / trunc_sqrt

	// yields specialization of fixed_point with integral bits necessary to store
	// the positive result of a square root operation on an object of type,
	// fixed_point<ReprType, Exponent>
	template <class FixedPoint>
	using trunc_sqrt_result = make_fixed_from_repr<
		_impl::make_unsigned<typename FixedPoint::repr_type>,
		(FixedPoint::integer_digits + 1) / 2>;

	// as trunc_sqrt_result but converts parameter, factor,
	// ready for safe sqrt operation
	template <class FixedPoint>
	trunc_sqrt_result<FixedPoint>
	constexpr trunc_sqrt(const FixedPoint & square) noexcept
	{
		using output_type = trunc_sqrt_result<FixedPoint>;
		return output_type(sqrt(square));
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::trunc_shift_left

	template <int Integer, class ReprType, int Exponent>
	constexpr fixed_point<ReprType, Exponent + Integer>
	trunc_shift_left(const fixed_point<ReprType, Exponent> & fp) noexcept
	{
		return fixed_point<ReprType, Exponent + Integer>::from_data(fp.data());
	};

	////////////////////////////////////////////////////////////////////////////////
	// sg14::trunc_shift_right

	template <int Integer, class ReprType, int Exponent>
	constexpr fixed_point<ReprType, Exponent - Integer>
	trunc_shift_right(const fixed_point<ReprType, Exponent> & fp) noexcept
	{
		return fixed_point<ReprType, Exponent - Integer>::from_data(fp.data());
	};

	////////////////////////////////////////////////////////////////////////////////
	// sg14::promote_multiply_result / promote_multiply

	// yields specialization of fixed_point with capacity necessary to store
	// result of a multiply between values of fixed_point<ReprType, Exponent>
	template <class Lhs, class Rhs = Lhs>
	using promote_multiply_result = promote_result<_impl::common_type<Lhs, Rhs>>;

	// as promote_multiply_result but converts parameter, factor,
	// ready for safe binary multiply
	template <class Lhs, class Rhs>
	promote_multiply_result<Lhs, Rhs>
		constexpr promote_multiply(const Lhs & lhs, const Rhs & rhs) noexcept
	{
		using result_type = promote_multiply_result<Lhs, Rhs>;
		return _impl::multiply<result_type>(lhs, rhs);
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::promote_divide_result / promote_divide

	// yields specialization of fixed_point with capacity necessary to store
	// result of a divide between values of fixed_point<ReprType, Exponent>
	template <class Lhs, class Rhs = Lhs>
	using promote_divide_result = promote_result<_impl::common_type<Lhs, Rhs>>;

	// as promote_divide_result but converts parameter, factor,
	// ready for safe binary divide
	template <class Lhs, class Rhs>
	promote_divide_result<Lhs, Rhs>
	constexpr promote_divide(const Lhs & lhs, const Rhs & rhs) noexcept
	{
		using result_type = promote_divide_result<Lhs, Rhs>;
		return _impl::divide<result_type>(lhs, rhs);
	}

	////////////////////////////////////////////////////////////////////////////////
	// sg14::promote_square_result / promote_square

	// yields specialization of fixed_point with integral bits necessary to store
	// result of a multiply between values of fixed_point<ReprType, Exponent>
	// whose sign bit is set to the same value
	template <class FixedPoint>
	using promote_square_result = make_ufixed<
		FixedPoint::integer_digits * 2,
		FixedPoint::fractional_digits * 2>;

	// as promote_square_result but converts parameter, factor,
	// ready for safe binary multiply-by-self
	template <class FixedPoint>
	promote_square_result<FixedPoint>
		constexpr promote_square(const FixedPoint & root) noexcept
	{
		using output_type = promote_square_result<FixedPoint>;
		using output_repr_type = typename output_type::repr_type;
		return output_type::from_data(
			_impl::shift_left<(FixedPoint::exponent * 2 - output_type::exponent), output_repr_type>(
				static_cast<output_repr_type>(root.data()) * static_cast<output_repr_type>(root.data())));
	}
}

#endif	// defined(_SG14_FIXED_POINT)
