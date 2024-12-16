#ifndef ADC7C93A_2878_4607_A1BE_693532202F69
#define ADC7C93A_2878_4607_A1BE_693532202F69

#include <limits>

/// @brief Uses max value of type T as a sentinel
/// @tparam T type of value
template <typename T>
class SentinelOptional {
   public:
	SentinelOptional() {}
	SentinelOptional(T init_value) : _value(init_value) {}

	const T& value() const { return _value; }
	T value_or(T default_value) const { return has_value() ? _value : default_value; }
	bool has_value() const { return _value != SENTINEL; }
	void clear() { _value = SENTINEL; }

	SentinelOptional& operator=(T new_value) {
		_value = new_value;
		return *this;
	}

	static constexpr T SENTINEL = std::numeric_limits<T>::max();

   private:
	T _value = SENTINEL;  // No value by default
};

#endif /* ADC7C93A_2878_4607_A1BE_693532202F69 */
