# Interworking with Google Mock

## Adding coroutines to a `TEST()` case

Our test will begin running as a function in the initial, or "main"
execution context, just as with Google Test/Mock.

We must explicitly declare a coroutine. The
`COROUTINE` macro is a _factory_ for coroutine objects.
It returns an instance of a coroutine. The optional argument to
`COROUTINE` is an identifying name.

#### Simple example
```
TEST(PainterTest, GoToPointTopLeft_GMS) {
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    
    auto coro = COROUTINE(MethodName) {
        MockCallHandle c = WAIT_FOR_CALL();
        EXPECT_TRUE( c.IS_CALL(mock_turtle, GoTo).With(Lt()) );
        c.RETURN();
    };

    coro.WATCH_CALL();

    painter.GoToPointTopLeft();
}
```
> [NOTE]
> We call `WATCH_CALL()` on a coroutine object when we are
> not calling it from within a coroutine.

After setting up the watch, we can call the code-under-test directly, to launch it.

> [TIP]
> `NEW_COROUTINE` may be used to create a coroutine on the heap; it returns a
> pointer to a coroutine object which should be freed using `delete`.

Let's try mixing watches with CTest expectations. We will launch `DrawDot()` which
will call `PenDown()` and `PenUp()`. Somewhat arbitrarily, one of these will be
handled by a coroutine and the other by an expectation.

#### Example with expectation 
```
TEST(PainterTest, DrawDot_GMS) {
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    
    auto coro = COROUTINE(MethodName) {
        // Similar to WillOnce(Return())
        WAIT_FOR_CALL().RETURN();
    };

    coro.WATCH_CALL(mock_turtle, PenDown);
    EXPECT_CALL(mock_turtle, PenUp);
    
    painter.DrawDot();
}
```
The calls do not in fact have to occur in order: `PenDown()` then `PenUp()`.
They could occur in the reverse order and this test would still pass.

In the above test, we could use a wildcarded watch, and check that the coroutine
got the right call inside the coroutine body.

> [TIP]
> This is how we can introduce Cotest gradually, or in a limited way, to
> existing test infrastructure.

> [NOTE]
> `WATCH_CALL` can be understood as Cotest's version of `EXPECT_CALL`:
> - It has the same syntax as `EXPECT_CALL` with the addition of wildcard usage.
> - It has full matcher support and can be used with `.With()`
> - It participates in mock call dispatch, and respects GMock's priority scheme.
> However:
> - It does not support action or cardinality extensions - these are determined
>   by the coroutine, which we will now discuss.

## Cardinality

It is normally an error for a coroutine to see a mock call after the
coroutine body has exited (we call the coroutine _oversaturated_).
A coroutine can elect to _retire_ before exit, in which case it will
be ignored for the purposes of mock call dispatch.
 
Let's suppose that we only require `DrawDot()` to call `PenDown()` and that
`PenUp()` is optional.

#### `RETIRE()` example 
```
TEST(PainterTest, Retire) {
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    
    auto coro = COROUTINE(MethodName) {
        WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
        RETIRE(); 
    };

    EXPECT_CALL(mock_turtle, PenUp).WillRepeatedly(Return());
    coro.WATCH_CALL();
    
    painter.DrawDot();
}
```
The wild-carded `WATCH_CALL()` would causes the coroutine to see
all mock calls until the coroutine calls `RETIRE()`. After this the
remaining mock call (`PenUp()`) is not shown to the coroutine so
the coroutine does not oversaturate. It is handled by the expectation. 

It is normally an error for a coroutine _not_ to have exited by the
time the test case completes (we call the coroutine _unsatisfied_).
We can suppress this error by saying `SATISFIED()` inside the coroutine
at the point at which further activity should become optional for a
test to pass.

#### `SATURATE()` example
```
TEST(PainterTest, Satisfy) {
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    
    auto coro = COROUTINE(Satisfy) {
        WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
        WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
        SATISFY(); 
        WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
        WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
    };

    coro.WATCH_CALL();
    
    painter.DrawDot();
}
```

In the above example, the code-under-test could drive another pen down/up cycle
before saturating the coroutine.

Cotest provides an interpretation of _cardinality_ for coroutines. This works
as follows:
 - There is one cardinality for each coroutine, regardless of the number of watches.
 - A coroutine is satisfied on exit, or earlier if `SATISFY()` is called.
 - A coroutine is saturated on exit, and then oversaturated if another call is seen
   - unless RETIRE() has been called. 

> [WARNING]
> If exiting a coroutine early, please use `EXIT_COROUTINE()` instead of `return`.
> This is to preserve compatibility with C++20.

## Multiple Test Coroutines

We can create as many coroutines as we requrire. Each will have its own
cardinality which we must respect. The priority scheme for mock call dispatch
will be respected - the last `WATCH_CALL` that matches will see the call
first and then we work backwards.

#### Example with multiple coroutines
```
TEST(PainterTest, TwoCoroutine) {
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    
    auto coro1 = COROUTINE(Coro1) {
        WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
    };
    auto coro2 = COROUTINE(Coro2) {
        WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
        RETIRE();
    };

    coro1.WATCH_CALL();
    coro2.WATCH_CALL();
    
    painter.DrawDot();
}
```
We needed to add `RETIRE()` in Coro2 because its watch will cause it to see
the `PenUp()` call, and we want it to ignore that call so that Coro1 can
handle it.

Let's try launching from one coroutine and handling one of the resulting mock
calls in another coroutine.

#### Handle mock in other cortoutine example
```
TEST(PainterTest, TwoCoroutineLaunch) {
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    
    auto coro1 = COROUTINE(Coro1) {
        WATCH_CALL();
        WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
    };
    auto coro2 = COROUTINE(Coro2) {
        WATCH_CALL();
        auto l = LAUNCH( painter.DrawDot() );
        WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
        WAIT_FOR_RESULT_FROM(l);
    };
}
```
Coro1 handles the `PenUp()` mock call that results from Coro1 launching
`DrawDot()`.

The reason this test does anything at all in spite of appearing not to
have a main body is that Cotest allows a coroutine to run when it is first
created. This is called _initial activity_. The coroutine will run until
its body waits for an event and an event has not already happened as
a result of a previous action such as a `LAUNCH` or `RETURN`. At this
point the coroutine yields and allows main to run.

In the example, Coro1 yields to main because it is waiting and no events
that it can see have occurred yet.

Coro2 performs a launch, which drives the mock call mechanism: it handles
`PenDown()`, then Coro1 handles `PenUp()` and then Coro2 handles the launch
result.
 
> [NOTE]
> Launch results must always be handled by the lauching coroutine. It is only
> mock calls that can be handled by any coroutine. This is a natural extension
> of Google Mock's mock handling philosophy.

> [WARNING]
> In test cases like this, it's important to check the declaration order of
> the coroutines. If the two coroutines in the example were exchanged, Coro2
> would begin its initial activity before Coro1 has been created. It is not
> possible for a coroutine to see a call that was made before the coroutine
> was created, because the corresponding `WATCH_CALL` must have completed before
> the mock call is made.

## Adding expectations to a `COTEST()` case

The `EXPECT_CALL()` macro can also be used in a Cotest test case. We will
return to [an example from the getting started guide](getting-started.md/#Loop inside test case example)
and add to this that the code-under-test will from time to time make a call
to `InkCheck()`. The ink check calls don't need to occur in any particualr
order relative to the mocks we are already checking for, so we would like to use
a Google Mock `EXPECT_CALL()` to handle them.

#### coro at lower prio example

#### coro at higher prio example

## What about the flexible example

If we require the `WATCH_CALL` to be wild-carded and at a higher priotity
than the `EXPECT_CALL`, we will need to begin using Cotest's [server style
API](server-style.md). The linked doc begins with this case.
