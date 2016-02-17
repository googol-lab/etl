//=======================================================================
// Copyright (c) 2014-2016 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#pragma once

#include <memory> //For shared_ptr

#include "etl/iterator.hpp"

namespace etl {

/*!
 * \brief A temporary expression base
 *
 * A temporary expression computes the expression directly and stores it into a temporary.
 */
template <typename D, typename V>
struct temporary_expr : comparable<D>, value_testable<D>, dim_testable<D>, gpu_delegate<V, D> {
    using derived_t         = D;                 ///< The derived type
    using value_type        = V;                 ///< The value type
    using memory_type       = value_type*;       ///< The memory type
    using const_memory_type = const value_type*; ///< The const memory type

    /*!
     * \brief The vectorization type for V
     */
    template<typename VV = default_vec>
    using vec_type = typename VV::template vec_type<value_type>;

    /*!
     * \brief Returns a reference to the derived object, i.e. the object using the CRTP injector.
     * \return a reference to the derived object.
     */
    derived_t& as_derived() noexcept {
        return *static_cast<derived_t*>(this);
    }

    /*!
     * \brief Returns a reference to the derived object, i.e. the object using the CRTP injector.
     * \return a reference to the derived object.
     */
    const derived_t& as_derived() const noexcept {
        return *static_cast<const derived_t*>(this);
    }

    //Apply the expression

    /*!
     * \brief Returns the element at the given index
     * \param i The index
     * \return a reference to the element at the given index.
     */
    value_type operator[](std::size_t i) const {
        return as_derived().result()[i];
    }

    /*!
     * \brief Returns the value at the given index
     * This function never alters the state of the container.
     * \param i The index
     * \return the value at the given index.
     */
    value_type read_flat(std::size_t i) const {
        return as_derived().result().read_flat(i);
    }

    /*!
     * \brief Returns the value at the given position (args...)
     * \param args The position indices
     * \return The value at the given position (args...)
     */
    template <typename... S, cpp_enable_if(sizeof...(S) == sub_size_compare<derived_t>::value)>
    value_type operator()(S... args) const {
        static_assert(cpp::all_convertible_to<std::size_t, S...>::value, "Invalid size types");

        return as_derived().result()(args...);
    }

    /*!
     * \brief Creates a sub view of the matrix, effectively removing the first dimension and fixing it to the given index.
     * \param i The index to use
     * \return a sub view of the matrix at position i.
     */
    template <typename DD = D, cpp_enable_if((sub_size_compare<DD>::value > 1))>
    auto operator()(std::size_t i) const {
        return sub(as_derived(), i);
    }

    /*!
     * \brief Perform several operations at once.
     * \param i The index at which to perform the operation
     * \tparam VV The vectorization mode to use
     * \return a vector containing several results of the expression
     */
    template<typename VV = default_vec>
    vec_type<VV> load(std::size_t i) const noexcept {
        return VV::loadu(memory_start() + i);
    }

    // Iterator

    /*!
     * \brief Return an iterator to the first element of the matrix
     * \return an const iterator pointing to the first element of the matrix
     */
    iterator<const derived_t> begin() const noexcept {
        return {as_derived(), 0};
    }

    /*!
     * \brief Return an iterator to the past-the-end element of the matrix
     * \return a const iterator pointing to the past-the-end element of the matrix
     */
    iterator<const derived_t> end() const noexcept {
        return {as_derived(), size(as_derived())};
    }

    // Direct memory access

    /*!
     * \brief Returns a pointer to the first element in memory.
     * \return a pointer tot the first element in memory.
     */
    memory_type memory_start() noexcept {
        return as_derived().result().memory_start();
    }

    /*!
     * \brief Returns a pointer to the first element in memory.
     * \return a pointer tot the first element in memory.
     */
    const_memory_type memory_start() const noexcept {
        return as_derived().result().memory_start();
    }

    /*!
     * \brief Returns a pointer to the past-the-end element in memory.
     * \return a pointer tot the past-the-end element in memory.
     */
    memory_type memory_end() noexcept {
        return as_derived().result().memory_end();
    }

    /*!
     * \brief Returns a pointer to the past-the-end element in memory.
     * \return a pointer tot the past-the-end element in memory.
     */
    const_memory_type memory_end() const noexcept {
        return as_derived().result().memory_end();
    }
};

/*!
 * \brief A temporary unary expression
 */
template <typename T, typename AExpr, typename Op, typename Forced>
struct temporary_unary_expr final : temporary_expr<temporary_unary_expr<T, AExpr, Op, Forced>, T> {
    static constexpr const bool is_not_forced = std::is_same<Forced, void>::value; ///< Indicate if the result is forced to an expression

    using value_type  = T;                                                                               ///< The value type
    using result_type = std::conditional_t<is_not_forced, typename Op::template result_type<AExpr>, Forced>; ///< The result type
    using data_type   = std::conditional_t<is_not_forced, std::shared_ptr<result_type>, result_type>;        ///< The data type

private:
    static_assert(is_etl_expr<AExpr>::value, "The argument must be an ETL expr");

    using this_type = temporary_unary_expr<T, AExpr, Op, Forced>;

    using get_result_op = std::conditional_t<is_not_forced, dereference_op, forward_op>;

    AExpr _a;               ///< The sub expression reference
    data_type _c;           ///< The result reference
    bool allocated = false; ///< Indicates if the temporary has been allocated
    bool evaluated = false; ///< Indicates if the expression has been evaluated

public:
    //Construct a new expression
    explicit temporary_unary_expr(AExpr a)
            : _a(a) {
        //Nothing else to init
    }

    //Construct a new expression
    temporary_unary_expr(AExpr a, std::conditional_t<is_not_forced, int, Forced> c)
            : _a(a), _c(c), allocated(true) {
        //Nothing else to init
    }

    //Copy an expression
    temporary_unary_expr(const temporary_unary_expr& e)
            : _a(e._a), _c(e._c), allocated(e.allocated), evaluated(e.evaluated) {
        //Nothing else to init
    }

    //Move an expression
    temporary_unary_expr(temporary_unary_expr&& e) noexcept
            : _a(e._a), _c(optional_move<is_not_forced>(e._c)), allocated(e.allocated), evaluated(e.evaluated){
        e.evaluated = false;
    }

    //Expressions are invariant
    temporary_unary_expr& operator=(const temporary_unary_expr& /*e*/) = delete;
    temporary_unary_expr& operator=(temporary_unary_expr&& /*e*/) = delete;

    //Accessors

    /*!
     * \brief Returns the sub expression
     * \return a reference to the sub expression
     */
    std::add_lvalue_reference_t<AExpr> a() {
        return _a;
    }

    /*!
     * \brief Returns the sub expression
     * \return a reference to the sub expression
     */
    cpp::add_const_lvalue_t<AExpr> a() const {
        return _a;
    }

    /*!
     * \brief Evaluate the expression, if not evaluated
     *
     * Will fail if not previously allocated
     */
    void evaluate() {
        if (!evaluated) {
            cpp_assert(allocated, "The result has not been allocated");
            Op::apply(_a, get_result_op::apply(_c));
            evaluated = true;
        }
    }

    /*!
     * \brief Evaluate the expression directly into the given result
     *
     * Will fail if not previously allocated
     */
    template <typename Result, typename F = Forced, cpp_disable_if(std::is_same<F, void>::value)>
    void direct_evaluate(Result&& r) {
        evaluate();
        r = result();
    }

    /*!
     * \brief Evaluate the expression directly into the given result
     *
     * Will fail if not previously allocated
     */
    template <typename Result, typename F = Forced, cpp_enable_if(std::is_same<F, void>::value)>
    void direct_evaluate(Result&& result) {
        Op::apply(_a, std::forward<Result>(result));
    }

    /*!
     * \brief Allocate the necessary temporaries, if necessary
     */
    template <typename F = Forced, cpp_disable_if(std::is_same<F, void>::value)>
    void allocate_temporary() {
        allocated = true;
    }

    /*!
     * \brief Allocate the necessary temporaries, if necessary
     */
    template <typename F = Forced, cpp_enable_if(std::is_same<F, void>::value)>
    void allocate_temporary() {
        if (!_c) {
            _c.reset(Op::allocate(_a));
        }

        allocated = true;
    }

    /*!
     * \brief Test if this expression aliases with the given expression
     * \param rhs The other expression to test
     * \return true if the two expressions aliases, false otherwise
     */
    template <typename E>
    bool alias(const E& rhs) const {
        return _a.alias(rhs);
    }

    /*!
     * \brief Returns the expression containing the result of the expression.
     * \return a const reference to the expression containing the result of the expression
     */
    result_type& result() {
        cpp_assert(evaluated, "The result has not been evaluated");
        cpp_assert(allocated, "The result has not been allocated");
        return get_result_op::apply(_c);
    }

    /*!
     * \brief Returns the expression containing the result of the expression.
     * \return a const reference to the expression containing the result of the expression
     */
    const result_type& result() const {
        cpp_assert(evaluated, "The result has not been evaluated");
        cpp_assert(allocated, "The result has not been allocated");
        return get_result_op::apply(_c);
    }

    result_type& gpu_delegate(){
        return result();
    }

    const result_type& gpu_delegate() const {
        return result();
    }

    bool gpu_delegate_valid() const noexcept {
        return evaluated && allocated;
    }
};

/*!
 * \brief A temporary binary expression
 */
template <typename T, typename AExpr, typename BExpr, typename Op, typename Forced>
struct temporary_binary_expr final : temporary_expr<temporary_binary_expr<T, AExpr, BExpr, Op, Forced>, T> {
    static constexpr const bool is_not_forced = std::is_same<Forced, void>::value; ///< Indicate if the result is forced to an expression

    using value_type  = T;                                                                                      ///< The value type
    using result_type = std::conditional_t<is_not_forced, typename Op::template result_type<AExpr, BExpr>, Forced>; ///< The result type
    using data_type   = std::conditional_t<is_not_forced, std::shared_ptr<result_type>, result_type>;               ///< The data type

private:
    static_assert(is_etl_expr<AExpr>::value && is_etl_expr<BExpr>::value, "Both arguments must be ETL expr");

    using this_type = temporary_binary_expr<T, AExpr, BExpr, Op, Forced>;

    using get_result_op = std::conditional_t<is_not_forced, dereference_op, forward_op>;

    AExpr _a;               ///< The left hand side expression reference
    BExpr _b;               ///< The right hand side expression reference
    data_type _c;           ///< The result reference
    bool allocated = false; ///< Indicates if the temporary has been allocated
    bool evaluated = false; ///< Indicates if the expression has been evaluated

public:
    //Construct a new expression
    temporary_binary_expr(AExpr a, BExpr b)
            : _a(a), _b(b) {
        //Nothing else to init
    }

    //Construct a new expression
    temporary_binary_expr(AExpr a, BExpr b, std::conditional_t<is_not_forced, int, Forced> c)
            : _a(a), _b(b), _c(c), allocated(true) {
        //Nothing else to init
    }

    //Copy an expression
    temporary_binary_expr(const temporary_binary_expr& e)
            : _a(e._a), _b(e._b), _c(e._c), allocated(e.allocated), evaluated(e.evaluated) {
        //Nothing else to init
    }

    //Move an expression
    temporary_binary_expr(temporary_binary_expr&& e) noexcept
            : _a(e._a), _b(e._b), _c(optional_move<is_not_forced>(e._c)), allocated(e.allocated), evaluated(e.evaluated) {
        e.evaluated = false;
    }

    //Expressions are invariant
    temporary_binary_expr& operator=(const temporary_binary_expr& /*e*/) = delete;
    temporary_binary_expr& operator=(temporary_binary_expr&& /*e*/) = delete;

    //Accessors

    /*!
     * \brief Returns the left-hand-side expression
     * \return a reference to the left hand side expression
     */
    std::add_lvalue_reference_t<AExpr> a() {
        return _a;
    }

    /*!
     * \brief Returns the left-hand-side expression
     * \return a reference to the left hand side expression
     */
    cpp::add_const_lvalue_t<AExpr> a() const {
        return _a;
    }

    /*!
     * \brief Returns the right-hand-side expression
     * \return a reference to the right hand side expression
     */
    std::add_lvalue_reference_t<BExpr> b() {
        return _b;
    }

    /*!
     * \brief Returns the right-hand-side expression
     * \return a reference to the right hand side expression
     */
    cpp::add_const_lvalue_t<BExpr> b() const {
        return _b;
    }

    /*!
     * \brief Evaluate the expression, if not evaluated
     *
     * Will fail if not previously allocated
     */
    void evaluate() {
        if (!evaluated) {
            cpp_assert(allocated, "The result has not been allocated");
            Op::apply(_a, _b, get_result_op::apply(_c));
            evaluated = true;
        }
    }

    /*!
     * \brief Evaluate the expression directly into the given result
     *
     * Will fail if not previously allocated
     */
    template <typename Result, typename F = Forced, cpp_disable_if(std::is_same<F, void>::value)>
    void direct_evaluate(Result&& r) {
        evaluate();
        r = result();
    }

    /*!
     * \brief Evaluate the expression directly into the given result
     *
     * Will fail if not previously allocated
     */
    template <typename Result, typename F = Forced, cpp_enable_if(std::is_same<F, void>::value)>
    void direct_evaluate(Result&& result) {
        Op::apply(_a, _b, std::forward<Result>(result));
    }

    /*!
     * \brief Allocate the necessary temporaries, if necessary
     */
    template <typename F = Forced, cpp_disable_if(std::is_same<F, void>::value)>
    void allocate_temporary() {
        allocated = true;
    }

    /*!
     * \brief Allocate the necessary temporaries, if necessary
     */
    template <typename F = Forced, cpp_enable_if(std::is_same<F, void>::value)>
    void allocate_temporary() {
        if (!_c) {
            _c.reset(Op::allocate(_a, _b));
        }

        allocated = true;
    }

    /*!
     * \brief Test if this expression aliases with the given expression
     * \param rhs The other expression to test
     * \return true if the two expressions aliases, false otherwise
     */
    template <typename E>
    bool alias(const E& rhs) const {
        return _a.alias(rhs) || _b.alias(rhs);
    }

    /*!
     * \brief Returns the expression containing the result of the expression.
     * \return a reference to the expression containing the result of the expression
     */
    result_type& result() {
        cpp_assert(evaluated, "The result has not been evaluated");
        cpp_assert(allocated, "The result has not been allocated");
        return get_result_op::apply(_c);
    }

    /*!
     * \brief Returns the expression containing the result of the expression.
     * \return a const reference to the expression containing the result of the expression
     */
    const result_type& result() const {
        cpp_assert(evaluated, "The result has not been evaluated");
        cpp_assert(allocated, "The result has not been allocated");
        return get_result_op::apply(_c);
    }

    result_type& gpu_delegate(){
        return result();
    }

    const result_type& gpu_delegate() const {
        return result();
    }

    bool gpu_delegate_valid() const noexcept {
        return evaluated && allocated;
    }
};

/*!
 * \brief Specialization for temporary_unary_expr.
 */
template <typename T, typename A, typename Op, typename Forced>
struct etl_traits<etl::temporary_unary_expr<T, A, Op, Forced>> {
    using expr_t = etl::temporary_unary_expr<T, A, Op, Forced>;
    using a_t    = std::decay_t<A>;

    static constexpr const bool is_etl                  = true;                           ///< Indicates if the type is an ETL type
    static constexpr const bool is_transformer          = false;                          ///< Indicates if the type is a transformer
    static constexpr const bool is_view                 = false;                          ///< Indicates if the type is a view
    static constexpr const bool is_magic_view           = false;                          ///< Indicates if the type is a magic view
    static constexpr const bool is_fast                 = etl_traits<a_t>::is_fast;       ///< Indicates if the expression is fast
    static constexpr const bool is_linear               = true;                           ///< Indicates if the expression is linear
    static constexpr const bool is_value                = false;                          ///< Indicates if the expression is of value type
    static constexpr const bool is_generator            = false;                          ///< Indicates if the expression is a generated
    static constexpr const bool needs_temporary_visitor = true;                           ///< Indicates if the expression needs a temporary visitor
    static constexpr const bool needs_evaluator_visitor = true;                           ///< Indicaes if the expression needs an evaluator visitor
    static constexpr const order storage_order          = etl_traits<a_t>::storage_order; ///< The expression storage order

    /*!
     * \brief Indicates if the expression is vectorizable using the
     * given vector mode
     * \tparam V The vector mode
     */
    template<vector_mode_t V>
    using vectorizable = std::true_type;

    /*!
     * \brief Returns the size of the given expression
     * \param v The expression to get the size for
     * \returns the size of the given expression
     */
    static std::size_t size(const expr_t& v) {
        return Op::size(v.a());
    }

    /*!
     * \brief Returns the dth dimension of the given expression
     * \param v The expression
     * \param d The dimension to get
     * \return The dth dimension of the given expression
     */
    static std::size_t dim(const expr_t& v, std::size_t d) {
        return Op::dim(v.a(), d);
    }

    /*!
     * \brief Returns the size of an expression of this fast type.
     * \returns the size of an expression of this fast type.
     */
    static constexpr std::size_t size() {
        return Op::template size<a_t>();
    }

    /*!
     * \brief Returns the Dth dimension of an expression of this type
     * \tparam D The dimension to get
     * \return the Dth dimension of an expression of this type
     */
    template <std::size_t D>
    static constexpr std::size_t dim() {
        return Op::template dim<a_t, D>();
    }

    /*!
     * \brief Returns the number of expressions for this type
     * \return the number of dimensions of this type
     */
    static constexpr std::size_t dimensions() {
        return Op::dimensions();
    }
};

/*!
 * \brief Specialization for temporary_binary_expr.
 */
template <typename T, typename A, typename B, typename Op, typename Forced>
struct etl_traits<etl::temporary_binary_expr<T, A, B, Op, Forced>> {
    using expr_t = etl::temporary_binary_expr<T, A, B, Op, Forced>;
    using a_t    = std::decay_t<A>;
    using b_t    = std::decay_t<B>;

    static constexpr const bool is_etl                  = true;                                                                                            ///< Indicates if the type is an ETL type
    static constexpr const bool is_transformer          = false;                                                                                           ///< Indicates if the type is a transformer
    static constexpr const bool is_view                 = false;                                                                                           ///< Indicates if the type is a view
    static constexpr const bool is_magic_view           = false;                                                                                           ///< Indicates if the type is a magic view
    static constexpr const bool is_fast                 = etl_traits<a_t>::is_fast && etl_traits<b_t>::is_fast;                                            ///< Indicates if the expression is fast
    static constexpr const bool is_linear               = true;                                                                                            ///< Indicates if the expression is linear
    static constexpr const bool is_value                = false;                                                                                           ///< Indicates if the expression is of value type
    static constexpr const bool is_generator            = false;                                                                                           ///< Indicates if the expression is a generated
    static constexpr const bool needs_temporary_visitor = true;                                                                                            ///< Indicates if the expression needs a temporary visitor
    static constexpr const bool needs_evaluator_visitor = true;                                                                                            ///< Indicaes if the expression needs an evaluator visitor
    static constexpr const order storage_order          = etl_traits<a_t>::is_generator ? etl_traits<b_t>::storage_order : etl_traits<a_t>::storage_order; ///< The expression storage order

    /*!
     * \brief Indicates if the expression is vectorizable using the
     * given vector mode
     * \tparam V The vector mode
     */
    template<vector_mode_t V>
    using vectorizable = std::true_type;

    /*!
     * \brief Returns the size of the given expression
     * \param v The expression to get the size for
     * \returns the size of the given expression
     */
    static std::size_t size(const expr_t& v) {
        return Op::size(v.a(), v.b());
    }

    /*!
     * \brief Returns the dth dimension of the given expression
     * \param v The expression
     * \param d The dimension to get
     * \return The dth dimension of the given expression
     */
    static std::size_t dim(const expr_t& v, std::size_t d) {
        return Op::dim(v.a(), v.b(), d);
    }

    /*!
     * \brief Returns the size of an expression of this fast type.
     * \returns the size of an expression of this fast type.
     */
    static constexpr std::size_t size() {
        return Op::template size<a_t, b_t>();
    }

    /*!
     * \brief Returns the Dth dimension of an expression of this type
     * \tparam D The dimension to get
     * \return the Dth dimension of an expression of this type
     */
    template <std::size_t D>
    static constexpr std::size_t dim() {
        return Op::template dim<a_t, b_t, D>();
    }

    /*!
     * \brief Returns the number of expressions for this type
     * \return the number of dimensions of this type
     */
    static constexpr std::size_t dimensions() {
        return Op::dimensions();
    }
};

/*!
 * \brief Prints a description of the temporary unary expr to the given stream
 * \param os The output stream
 * \param expr The expression to print
 * \return the output stream
 */
template <typename T, typename AExpr, typename Op, typename Forced>
std::ostream& operator<<(std::ostream& os, const temporary_unary_expr<T, AExpr, Op, Forced>& expr) {
    return os << Op::desc() << "(" << expr.a() << ")";
}

/*!
 * \brief Prints a description of the temporary binary expr to the given stream
 * \param os The output stream
 * \param expr The expression to print
 * \return the output stream
 */
template <typename T, typename AExpr, typename BExpr, typename Op, typename Forced>
std::ostream& operator<<(std::ostream& os, const temporary_binary_expr<T, AExpr, BExpr, Op, Forced>& expr) {
    return os << Op::desc() << "(" << expr.a() << ", " << expr.b() << ")";
}

} //end of namespace etl
