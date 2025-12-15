#include <cstdio>
#include <cstring>
#include <chrono>
#include "../components/serializer/sg_serializer.h"

static int expect_contains(const char* hay, const char* needle) {
  if (!strstr(hay, needle)) {
    std::fprintf(stderr, "[FAIL] missing '%s' in: %s\n", needle, hay);
    return 1;
  }
  return 0;
}

int main() {
  int fails = 0;
  SgSerCtx ctx;
  sg_ser_init(&ctx, "sg-test", 1);

  // seq should start at 1 after next_seq
  sg_ser_next_seq(&ctx);
  SgSerOccupancy occ{1234, 1, 0.9f};
  char buf[256];
  sg_ser_make_occupancy(&ctx, &occ, buf, sizeof(buf));
  fails |= expect_contains(buf, "\"device_id\":\"sg-test\"");
  fails |= expect_contains(buf, "\"seq\":1");
  fails |= expect_contains(buf, "\"type\":\"occupancy\"");
  fails |= expect_contains(buf, "\"ts_iso\":\"1970-01-01T");

  // seq increments
  sg_ser_next_seq(&ctx);
  SgSerHealth h{2000, 2, -42};
  sg_ser_make_health(&ctx, &h, buf, sizeof(buf));
  fails |= expect_contains(buf, "\"seq\":2");
  fails |= expect_contains(buf, "\"type\":\"health\"");

  // bench-ish: build envelopes in a tight loop to ensure no crash
  auto t0 = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 5000; ++i) {
    sg_ser_next_seq(&ctx);
    sg_ser_envelope(&ctx, (uint32_t)i, "meas", "{\"ok\":true}", buf, sizeof(buf));
  }
  auto t1 = std::chrono::high_resolution_clock::now();
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
  std::printf("[INFO] Built 5000 envelopes in %lld us\n", (long long)us);

  if (fails) {
    std::fprintf(stderr, "serializer_test FAILED with %d issue(s)\n", fails);
    return 1;
  }
  std::printf("serializer_test OK\n");
  return 0;
}
