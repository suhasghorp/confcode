struct Args
{
    void * addr = nullptr;
    size_type len = 0;
    int fd = -1;
    size_type offset = 0;
    bool magic = false;
    size_type page_size = 0;
};
