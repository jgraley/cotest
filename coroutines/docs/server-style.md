# Cotest Server Style

A few of Cotest's API functions are designated as being the server-style API. This part of the API is simple to use, but a rule applies to its usage.

The server API functions are
 - `NEXT_EVENT()` - this returns the next event received by the coroutine
 - `ACCEPT()` - tells Cotest that this coroutine will handle the mock call
 - `DROP()` - tells Cotest the corotuine will not handle the call and GMock should continue searching for a handler

The rule is that once we have a handle from `NEXT_EVENT()` we must call `DROP()` or `ACCEPT()` on that handle before making any launches or returning from any other mock calls. It is OK to return on the handle provided by `NEXT_EVENT()` because in this case `ACCEPT()` in inferred.

We now return to the example given at the end of [Interworking with Google Mock](working-with-gmock.md) in which we're testing an algorithm that uses a turtle to draw a square, and we have certain constraints:
 - Handle intermittent calls to `InkCheck()`
 - the `WATCH_CALL()` to be wild-carded and at a higher priority than the `EXPECT_CALL()` and
 - flexibility in the number of iterations of the loop

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
 - Any other: Not for us to handle. We will drop the call and jump back to `event = NEXT_EVENT();`

 Note that we didn't need to call `event.ACCEPT()` necause in each case, we immediately call `event.RETURN()`. However, it
 is good practice to get to a `DROP()` or `ACCEPT()` reasonably soon, because of the limitations on what we can do in the meantime.

> [!NOTE]
> Cotest's `WAIT_FOR_` are built on the server API. They use NEXT_EVENT() to get events. If the event is desired, they
> accept the call and return the handle, otherwise they drop the call and try again. 

 - How the WAIT_X() macros work to the server style doc, however.
 - Discuss the examples in cotest-serverised.cc
 - Discuss further options: global modality, multiple servers
