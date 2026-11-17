enum Flags : std::uint16_t
{
    SingleProducer = 0x0001,
    MultiProducer = 0x0002,
    SingleProducerBlocks = 0x0004,
    MultiProducerBlocks = 0x0008,
    SingleConsumer = 0x0010,
    MultiConsumer = 0x0020,
    SingleConsumerBlocks = 0x0040,
    MultiConsumerBlocks = 0x0080,
    Interprocess = 0x0100,
    Multicast = 0x0200,
    Broadcast = Multicast,
    None = 0x0000,
    All = 0xffff,
};
