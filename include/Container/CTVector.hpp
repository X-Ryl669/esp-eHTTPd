#ifndef hpp_CTVector_hpp
#define hpp_CTVector_hpp

namespace Container
{
    /** Used for compiling, to show what a type is (the compiler will error out when trying to instantiate a undefined template) */
    template <typename T> struct DumpTypeAtCompile;
    /** A plain old dumb typelist used to build variadic templates and functions arguments */
    template <typename...Types> struct TypeList{ static constexpr std::size_t size = sizeof...(Types); };

    /** Transform the given typelist with the given type that must contain a 'Type' type */
    template <template<typename> class Transformer, typename... Types> struct TransformTypeList;
    template <template<typename> class Transformer> struct TransformTypeList<Transformer> { using Type = TypeList<>; };
    template <template<typename> class Transformer, typename Head, typename... Tail> struct TransformTypeList<Transformer, Head, Tail...> { using Type = TypeList<typename Transformer<Head>::Type, typename Transformer<Tail>::Type...>; };


    /** Type list manipulation tools used to transform the complete argument list to the expected argument list */
    // Merge typelists
    template <typename T, typename U> struct Merge { typedef TypeList<T, U> Type; };
    template <typename ... T, typename ... U> struct Merge<TypeList<T...>, TypeList<U...>> { typedef TypeList<T..., U...> Type; };
    template <typename T, typename ... U> struct Merge<T, TypeList<U...>> { typedef TypeList<T, U...> Type; };
    template <typename ... T, typename U> struct Merge<TypeList<T...>, U> { typedef TypeList<T..., U> Type; };
    // Get the N-th type in the list
    template <std::size_t index, typename T, typename... TS> struct NthInner { using Type = typename NthInner<index - 1, TS...>::Type; };
    template <typename T, typename... TS> struct NthInner<0, T, TS...> { using Type = T; };

    template <std::size_t index, typename U> struct Nth {};
    template <std::size_t index, typename ... TS> struct Nth<index, TypeList<TS...> > { using Type = typename NthInner<index, TS...>::Type; };

    // Replace the Nth to Mth type with the given type list
    template<std::size_t O, class...> class wrapper{}; // This is used as a catchall to help the compiler deduce the template's argument
    template <typename T, std::size_t Offset, std::size_t... Is> TypeList<typename Nth<Is + Offset, T>::Type...> Selector(wrapper<Offset, T, std::index_sequence<Is...>>);
    template <std::size_t N, std::size_t M, typename T, typename U> struct Replace {};
    template <std::size_t N, std::size_t M, typename ... T, typename ... U>
    struct Replace<N, M, TypeList<T...>, TypeList<U...>>
    {
        using IndicesBefore = std::make_index_sequence<N>;
        using IndicesAfter = std::make_index_sequence<sizeof...(U)-M>;
        using First = decltype(Selector(wrapper<0, TypeList<U...>, IndicesBefore>{}));
        using Last = decltype(Selector(wrapper<M, TypeList<U...>, IndicesAfter>{}));
        using Type = typename Merge<First, typename Merge<TypeList<T...>, Last>::Type>::Type;
    };
    template <std::size_t N, std::size_t M, typename T, typename ... U>
    struct Replace<N, M, T, TypeList<U...>>
    {
        using IndicesBefore = std::make_index_sequence<N>;
        using IndicesAfter = std::make_index_sequence<sizeof...(U)-M>;
        using First = decltype(Selector(wrapper<0, TypeList<U...>, IndicesBefore>{}));
        using Last = decltype(Selector(wrapper<M, TypeList<U...>, IndicesAfter>{}));
        using Type = typename Merge<First, typename Merge<T, Last>::Type>::Type;
    };
    // A flatten operator, that loops other all types in the type list, merge the resulting "meta" type's Type and returns that
    template <typename... T> struct FlattenInner { using Type = TypeList<>; }; // End recursion
    template <typename... T> struct FlattenInner<TypeList<T...>> { using Type = TypeList<T...>; };
    template <typename... T0, typename... T1, typename... T2> struct FlattenInner<TypeList<T0...>, TypeList<T1...>, T2...> { using Type = typename FlattenInner<TypeList<T0..., T1...>, T2...>::Type; };

    template<typename T> struct Flatten { using Type = TypeList<T>; };
    template<typename... TT> struct Flatten<TypeList<TT...>> { using Type = typename FlattenInner<typename Flatten<TT>::Type...>::Type; };




    /** A Compile time vector is an array with possible type manipulation (like adding more data in the array or transforming the array data by some predefined method) */
    template <typename ... List>
    struct ConcreteCTVector
    {
        std::tuple<List...> array;

    };

    /** Make sure the given array only contains unique elements in it (static assert if false) */
    template <typename T, std::size_t N>
    struct CTUniqueSet
    {
        std::array<T, N> array;
        consteval CTUniqueSet(auto... ts): array{std::move(ts)...} {
            if (N == 0) return;
            for (auto i = array.begin(); i != array.end(); ++i)
            {
                auto j = i + 1;
                while (j != array.end())
                    if (*j == *i)
                        // If compiler stops here, you've 2 identical value in your array, remove the duplicate and try again
                        throw "not unique element found";
                    else ++j;
            }
        }
    };
    CTUniqueSet(auto t, auto... ts) -> CTUniqueSet<decltype(t), sizeof...(ts) + 1>;



    template <typename T, std::size_t N, std::size_t M>
    consteval std::array<T, N+M> mergeArrays(std::array<T, N> a, std::array<T, M> b)
    {
        std::array<T, N+M> array = {};
        std::size_t index = 0;
        for (auto& el : a) array[index++] = std::move(el);
        for (auto& el : b) array[index++] = std::move(el);
        return array;
    }


    template <typename T, std::size_t N>
    consteval std::size_t countUniqueElements(std::array<T, N> array)
    {
        std::size_t n = N;
        if (n == 0) return n;
        for (auto i = array.cbegin(); i < array.end(); ++i)
        {
            auto j = i + 1;
            while (j != array.end()) {
                const T ii = *i, jj = *j;
                if (jj == ii) { n--; break; }
                else ++j;
            }
        }
        return n;
    }

    template <auto a, auto b>
    consteval auto getUnique()
    {
        using T = std::decay_t<decltype(a[0])>;
        constexpr auto N = a.size(), M = b.size();
        constexpr std::array<T, N+M> array = mergeArrays(a, b);
        constexpr std::size_t u = countUniqueElements(array);
        std::size_t n = 0;
        std::array<T, u> ret = {};
        if (N == 0) return ret;
        for (auto i = array.begin(); i != array.end(); ++i)
        {
            auto j = i + 1;
            bool unique = true;
            while (j != array.end())
                if (*j == *i) { unique = false; break; }
                else ++j;

            if (unique) ret[n++] = *i;
        }
        return ret;
    }


    // Convert from a std::array of enum value to a type with the given implementation
    template <template<auto> class Transformer, auto array>
    constexpr auto makeTypes()
    {
        return []<std::size_t ... i>(std::index_sequence<i ...>) { return TypeList<typename Transformer<array[i]>::Type...>{}; }(std::make_index_sequence<array.size()>{});
    }

    template <template<auto> class Transformer, auto array, auto arrayToMergeWith>
    constexpr auto withMinimumTypes()
    {
        // If the compiler breaks here, it's because the given array doesn't contain unique element. Make sure there is no duplicate in the array above
        constexpr auto isUnique = []<std::size_t ... i>(std::index_sequence<i ...>) { return CTUniqueSet{ array[i]... }; }(std::make_index_sequence<array.size()>{});
        constexpr auto merged = getUnique<array, arrayToMergeWith>();
        return makeTypes<Transformer, merged>();
    }

}

#endif
