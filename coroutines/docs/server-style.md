# Cotest Server Style

## Server style API

A few of Cotest's API functions are designated as forming the _server-style API_. These are simple to use, but a rule applies to their usage which we must be careful to respect.

The server API functions are
 - `NEXT_EVENT()` - this returns the next event received by the coroutine (can be mock call or launch result)
 - `ACCEPT()` - tells Cotest that this coroutine will handle the mock call
 - `DROP()` - tells Cotest the corotuine will not handle the call and GMock should continue searching for a handler

The rule is that if we call `NEXT_EVENT()` and it returns a mock call handle, we must call `DROP()` or `ACCEPT()` on that handle before making any launches or returning from any other mock calls. It is also OK to `RETURN()` on the handle provided by `NEXT_EVENT()` because in this case `ACCEPT()` in inferred.

The handle returned by `NEXT_EVENT()` is an event handle, and can represent a mock call or a launch result:
 - `event.IS_CALL()` will return a valid handle (casts to `true`) if it's a mock call.
 - `event.IS_RETURN()` will return a valid handle (casts to `true`) if it's a launch return.

## Returning to the square-drawing test case

We now return to the example given at the end of [Interworking with Google Mock](working-with-gmock.md) in which we're testing an algorithm that uses a turtle to draw a square, and we have certain constraints:
 - The test must handle intermittent calls to `InkCheck()`.
 - The `WATCH_CALL()` must be wild-carded and at a higher priority than the `EXPECT_CALL()`.
 - We must demonstrate flexibility in the number of "sides" that the code-under-test actually draws.

#### Flexible with ink check example
```
COTEST(PainterTest, SquareInkChecks3)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    EXPECT_CALL(mock_turtle, InkCheck).WillRepeatedly(Return());
    WATCH_CALL();

    auto l = LAUNCH( painter.DrawSquareInkChecks(5) );
    WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
    EventHandle event;
    while(true)
    {
        event = NEXT_EVENT();
        if( event.IS_CALL(mock_turtle, PenUp) ) {
            event.ACCEPT();
            break;
        }
        else if( event.IS_CALL(mock_turtle, Forward) ) {
            event.ACCEPT();
        }
        else {
            event.DROP();
            continue;
        }
        
        EXPECT_TRUE( event.IS_CALL(mock_turtle, Forward(5)) );
        event.RETURN();
        WAIT_FOR_CALL(mock_turtle, Turn(90)).RETURN();
    }
    event.RETURN();
    WAIT_FOR_RESULT();
    SATISFY(); // Workaround issue #11
}
```
Server style begins with `event = NEXT_EVENT();`. We then enter an if-else-if chain that covers the three possible cases:
 - `PenUp()`: signifies code-under-test is finishing. We want to handle this so we accept and break out of our loop.
 - `Forward()`: signifies code-under-test is continuing. We want to handle this so we accept and let our loop body run.
 - Any other: not for us to handle. We will drop the call and jump back to `event = NEXT_EVENT();`

 We didn't need to call `event.ACCEPT()` because in each case, we immediately call `event.RETURN()`. However, it
 is good practice to get to a `DROP()` or `ACCEPT()` as soon as practiable.

> [!NOTE]
> Cotest's `WAIT_FOR_` macros are built on the server API. They use `NEXT_EVENT()` to get events. If the event is desired, they
> accept it (if a call) and return the handle, otherwise they drop it and try again.

## Server-style tests

This part of Cotest's API is called _server style_ because it permits (or even seems to encourage) a style of test case in which we poll for incoming events inside a loop and then dispatch them to handlers depending on which event was received. 

In [the server-style examples](../test/cotest-serverised.cc) there are two examples in this style. `Example1` runs an event loop that can respond in four different ways to incoming events:
 - Call to `Mock1()`: argument value is checked and call is returned.
 - Call to `Mock2()`: argument value is checked, call is returned, and then a call to `MockExtra()` is expected and returned.
 - Call to `Mock3()`: this server drops it for something else to handle
 - Launch result: check return value and then finish the test.

This test mostly doesn't care in what order mock calls are made (just as a server doesn't care in what order clients send requests) but we have been able to enforce a conditional sub-sequence requirement: that one such call is always followed by a specific subsequent call.

`Example2` goes on to repeat the same exercise, but now all the calls go to `Mock1()` and are differentiated by the value of the supplied argument. We `switch` on that value and perform the same actions as with `Example1`. This shows that we can use `GetArg<>()` to extract arg values before deciding whether to drop or accept.

Other techniques that could be used with server-style tests include:
 - Global modality: multiple event loops are coded, one for each of a number of _modes_, and when we want to switch mode we jump between the loops.
 - Multiple servers: manage complexity by splitting into multiple server loops, one coroutine each, and have the higher priority ones drop calls for other ones to handle.
 - Trigger launches based on incoming watch calls. The server can deal with completion as and when it happens.
