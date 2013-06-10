#include "gtest/gtest.h"
#include "utilities/pointpicker.h"
#include "model/pointcloud.h"

#include <QTime>
#include <QDebug>

namespace {

// The fixture for testing class Foo.
class PerfTest : public ::testing::Test {
 protected:

    QTime t;
    int ms;

    PerfTest() {
        ms = 0;
    }

    virtual ~PerfTest() {}

    virtual void SetUp() {
        t.restart();
        t.start();
    }

    virtual void TearDown() {
        int ms = t.elapsed();
    }
};

TEST_F(PerfTest, DoesXyz) {
  EXPECT_EQ(0, 0);
  qDebug() << "Time (ms): " << ms;
}

TEST(PTXTest, Read) {
  PointCloud pc;
  bool succ = pc.load_ptx("/home/rickert/Masters/utilities/ptxmaker/out.ptx");
  EXPECT_EQ(succ, true);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
