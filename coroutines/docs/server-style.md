Layout
 - Introduce the calls
 - Rules about prompt drop/accept

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



 - How the WAIT_X() macros work to the server style doc, however.
 - Discuss the examples in cotest-serverised.cc
 - Discuss further options: global modality, multiple servers
