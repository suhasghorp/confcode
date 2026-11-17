auto to_promise = [](auto& promise) {
  return [&](auto packet) {
    promise.yield_value(packet);
    return true;
  };
};
