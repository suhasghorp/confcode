void my_function() {
    CTRACK;
// Expands to:
EventHandler _ctrack_instance{__LINE__,
__FILE__, __FUNCTION__};

    // Your code here
    if (error) return;
// Destructor called here

    // More code
}  // Destructor called here
