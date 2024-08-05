# Event timers

An `EventTimers` object is a collection of `EventTimer`s.

Each `EventTimer` represents a timer that can be started by publishing a
`TimerRequest` event to the `PubSub`.

A timer is identified by a `pw_tokenizer` token. If a request is handled
while another request for the same token is pending, the previous request is
replaced by the new one.

After the timeout given by the request, this object will publish a corresponding
`TimerExpired` event.
