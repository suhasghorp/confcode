ProcessId
ProcessId::
current()
{
    static ProcessId id = [] {
        ::pthread_atfork(
            nullptr, // prepare
            nullptr, // parent
            [] { id = ProcessId{::getpid()}; });
        return ProcessId{::getpid()};
    }();
    return id;
}
