# Interworking with Google Mock

Please see [the test case for the examples](/coroutines/test/examples-for-docs.cc) for code-under-test and mocking assets

## Adding coroutines to a `TEST()` case

Our test case will begin running in the "main" execution context, just as with Google Test/Mock.

We must explicitly declare a coroutine. The
`COROUTINE` macro is a _factory_ for coroutine objects and it returns an instance of a coroutine. The optional argument to
`COROUTINE` is an optional name for the coroutine (no quotes required).

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
> [!NOTE]
> We call `WATCH_CALL()` on a coroutine object when we are
> not calling it from within a coroutine.

After declaring assets, coroutine and setting up the watch, we can launch the code-under-test by calling it directly as with Google Test. We call this _launching from main_.

> [!TIP]
> `NEW_COROUTINE` may be used to create a coroutine on the heap; it returns a
> pointer to a coroutine object which should be freed using `delete`.

Let's try mixing watches with Google Mock expectations. We will launch `DrawDot()` which
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
They could occur in the reverse order and this test would still pass. In the above test, we could use a wildcarded watch, and check that the coroutine got the correct call inside the coroutine body.

> [!TIP]
> This is how we can introduce Cotest gradually, or in a limited way, to
> legacy test cases.

> [!NOTE]
> `WATCH_CALL` can be understood as Cotest's version of `EXPECT_CALL`:
> - It has the same syntax as `EXPECT_CALL` with the addition of wildcard usage.
> - It supports matcher speecification including `.With()`
> - It participates in mock call dispatch, and respects GMock's priority scheme.
> However:
> - It does not support action or cardinality specifications - these are determined
>   by the coroutine, which we will now discuss.  

## Cardinality

It is normally an error for a coroutine to see a mock call after the coroutine body has exited (we call the coroutine _oversaturated_). This is to avoid allowing a unit test to pass in an ambiguous scenario.
However, a coroutine can elect to _retire_ before exit, in which case it will be ignored for the purposes of mock call dispatch.
 
Let's suppose that we want the coroutine to handle `PenDown()` and an
expectation to handle `PenUp()`. One way to do this is to give the coroutine 
a higher priority but have it retire when it's processd the `PenDown()` message.

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
The wild-carded `WATCH_CALL()` causes the coroutine to see
all mock calls until the coroutine calls `RETIRE()`. After this the
remaining mock call (`PenUp()`) is not shown to the coroutine so
the coroutine does not oversaturate. 

It is normally an error for a coroutine _not_ to have exited by the
time the test case completes (we call the coroutine _unsatisfied_).
We can suppress this error by saying `SATISFY()` inside the coroutine
at the point at which further activity should become optional for a
test to pass.

#### `SATISFY()` example
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
 - A coroutine is saturated on exit, and then oversaturated if another call is seen unless `RETIRE()` has been called. 

> [!WARNING]
> If exiting a coroutine early, please use `EXIT_COROUTINE()` instead of `return`: this is to preserve compatibility with C++20 coroutines.

## Multiple Test Coroutines

We can create as many coroutines as we require. Each will have its own cardinality and GMock's priority scheme for mock call dispatch will be respected. 

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
Two coroutines have been declared and each has been given a watch on any mock call. The watches do not have to be declared in the same order as the coroutines. 

In this test case, `Coro2` has the higher priority and will see mock calls before `Coro1`, so we need to add `RETIRE()` so that `Coro1` gets to see the second mock call, which we expect to be `PenUp()`.

Let's try launching code-under-test from one coroutine and handling a resulting mock call in a different coroutine.

#### Handle mock in other coroutine example
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
`Coro1` handles the `PenUp()` mock call that results from `Coro2` launching
`DrawDot()`.

The reason this test does anything at all in spite of appearing not to
have a main body is that Cotest allows a coroutine to run when it is first
created. This is called _initial action_. At the point of instantiation, every coroutine will run until it is _blocked_, at which point it will yield and allow main to continue running. A coroutine is blocked when
 - it waits for an event and
 - an event has not already happened as a result of a previous `LAUNCH` or `RETURN`.

Coroutines yield when blocked in order to allow the test to continue: other corotuines can perform initial actions and/or main context can launch code-under-test.

In the example, `Coro1` is immediately blocked because it is waiting and no events
that it can see have occurred yet, so it yields to main. `Coro2` performs a launch, which drives the mock call mechanism: it handles `PenDown()`, then Coro1 handles `PenUp()` and then `Coro2` handles the launch result. `Coro2` is never blocked in this example because both of its `WAIT_FOR_` calls are waiting for events that have already happened. When `Coro2` exits, it yields and the test case completes (permitting GMock to perform end-of-scope cardinality checks).
 
> [!NOTE]
> Launch results must always be handled by the lauching coroutine. It is only
> mock calls that can be handled by other coroutines. This is a natural extension
> of Google Mock's mock handling philosophy.

> [!WARNING]
> In test cases like this, it's important to check the declaration order of
> the coroutines. If the two coroutines in the example were exchanged, Coro2
> would begin its initial action before Coro1 has been created. It is not
> possible for a coroutine to see a call that was made before the coroutine
> was created, because the corresponding `WATCH_CALL` must have completed before
> the mock call is made.

## Adding expectations to a `COTEST()` case

The `EXPECT_CALL()` macro can also be used in a Cotest test case. We will
return to [an example from the getting started guide](getting-started.md#loop-inside-test-case-example)
and add to this that the code-under-test will from time to time make a call
to `InkCheck()`. The ink check calls don't need to occur in any particular
order relative to the mocks we are already checking for, so we would like to use
a Google Mock `EXPECT_CALL()` to handle them.

#### Coroutine at lower prio example

The solution in GMock is to add a separate expectation for these calls (or use `ON_CALL()`). In Cotest, we can do the same:
```
COTEST(PainterTest, SquareInkChecks1)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);

    WATCH_CALL();
    EXPECT_CALL(mock_turtle, InkCheck).WillRepeatedly(Return());

    auto l = LAUNCH( painter.DrawSquareInkChecks(5) );
    WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
    for( int i=0; i<4; i++ )
    {
        WAIT_FOR_CALL(mock_turtle, Forward(5)).RETURN();
        WAIT_FOR_CALL(mock_turtle, Turn(90)).RETURN();
    }
    WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
    WAIT_FOR_RESULT();
}
```

The new code-under-test method `DrawSquareInkChecks()` makes calls to `InkCheck()` and we want to absorb them without error. 
Since the `EXPECT_CALL()` comes after the `WATCH_CALL()` it has a higher priority. This means the coroutine will not see the call if the expectation handles it, and in this case the expectation handles all calls to `InkCheck()`.

#### Coroutine at higher prio example

We could reverse the priorities if we wanted:
```
COTEST(PainterTest, SquareInkChecks2)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);

    EXPECT_CALL(mock_turtle, InkCheck).WillRepeatedly(Return());
    WATCH_CALL();

    ... From here, same as the previous example ...
```

This works because we use interior filtering: the `WAIT_FOR_CALL(obj, call)` will drop any mock call that does not match, including the `InkCheck()` calls, and the expectation will handle them.

## What about the flexible example?

If we require
 - handling of `InkCheck()`,
 - the `WATCH_CALL` to be wild-carded and at a higher priority than the `EXPECT_CALL` and
 - [flexibility in the number of iterations of the loop](getting-started.md#flexible-test-example)
   
then we will need to begin using Cotest's [server style API](server-style.md) which is a lower-level and more flexible way of using Cotest.
