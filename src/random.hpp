#pragma once

#include <algorithm>
#include <random>
#include <type_traits>

extern std::default_random_engine rng;

template <typename IntType>
std::enable_if_t<std::is_integral_v<IntType>, IntType> rand(IntType min, IntType max)
{
    using DistType = std::uniform_int_distribution<IntType>;
    static DistType dist;
    return dist(rng, typename DistType::param_type(min, max));
}

template <typename FloatType>
std::enable_if_t<std::is_floating_point_v<FloatType>, FloatType> rand(FloatType min, FloatType max)
{
    using DistType = std::uniform_real_distribution<FloatType>;
    static DistType dist;
    return dist(rng, typename DistType::param_type(min, max));
}

template <class, class Enable = void>
struct is_iterator : std::false_type {
};

template <typename T>
struct is_iterator<T,
    typename std::enable_if_t<std::is_base_of<std::input_iterator_tag,
                                  typename std::iterator_traits<T>::iterator_category>::value
        || std::is_same<std::output_iterator_tag,
            typename std::iterator_traits<T>::iterator_category>::value>> : std::true_type {
};

template <typename Iterator>
std::enable_if_t<is_iterator<Iterator>::value, Iterator> rand(Iterator begin, Iterator end)
{
    const auto size = std::distance(begin, end);
    assert(size > 0);
    if (size == 0)
        return begin;
    return std::next(begin, rand<decltype(size)>(0, size - 1));
}

template <typename Container>
auto rand(Container&& container) -> decltype(*container.begin())
{
    return *rand(
        std::forward<Container>(container).begin(), std::forward<Container>(container).end());
}

template <typename T>
T rand();

template <>
inline float rand()
{
    static std::uniform_real_distribution<float> dist(0.f, 1.f);
    return dist(rng);
}

template <>
inline bool rand()
{
    static std::uniform_int_distribution<int> dist(0, 1);
    return dist(rng) == 1;
}

template <typename Container>
void shuffle(Container&& container)
{
    std::shuffle(
        std::forward<Container>(container).begin(), std::forward<Container>(container).end(), rng);
}
