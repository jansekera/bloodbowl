#include <gtest/gtest.h>
#include "bb/position.h"

using namespace bb;

TEST(Position, OnPitch) {
    EXPECT_TRUE((Position{0, 0}).isOnPitch());
    EXPECT_TRUE((Position{25, 14}).isOnPitch());
    EXPECT_TRUE((Position{12, 7}).isOnPitch());
    EXPECT_FALSE((Position{-1, 0}).isOnPitch());
    EXPECT_FALSE((Position{26, 0}).isOnPitch());
    EXPECT_FALSE((Position{0, -1}).isOnPitch());
    EXPECT_FALSE((Position{0, 15}).isOnPitch());
}

TEST(Position, EndZone) {
    EXPECT_TRUE((Position{0, 5}).isInEndZone(true));   // home endzone
    EXPECT_FALSE((Position{0, 5}).isInEndZone(false));
    EXPECT_TRUE((Position{25, 5}).isInEndZone(false));  // away endzone
    EXPECT_FALSE((Position{25, 5}).isInEndZone(true));
    EXPECT_FALSE((Position{12, 7}).isInEndZone(true));
    EXPECT_FALSE((Position{12, 7}).isInEndZone(false));
}

TEST(Position, WideZone) {
    // Wide zones: y 0-3 and y 11-14
    EXPECT_TRUE((Position{5, 0}).isInWideZone());
    EXPECT_TRUE((Position{5, 3}).isInWideZone());
    EXPECT_FALSE((Position{5, 4}).isInWideZone());
    EXPECT_FALSE((Position{5, 10}).isInWideZone());
    EXPECT_TRUE((Position{5, 11}).isInWideZone());
    EXPECT_TRUE((Position{5, 14}).isInWideZone());
}

TEST(Position, ChebyshevDistance) {
    Position a{5, 5};
    EXPECT_EQ(a.distanceTo({5, 5}), 0);
    EXPECT_EQ(a.distanceTo({6, 5}), 1);
    EXPECT_EQ(a.distanceTo({6, 6}), 1);  // diagonal = 1
    EXPECT_EQ(a.distanceTo({8, 7}), 3);  // max(3, 2) = 3
    EXPECT_EQ(a.distanceTo({5, 10}), 5);
}

TEST(Position, Adjacent) {
    Position center{10, 7};
    auto adj = center.getAdjacent();
    EXPECT_EQ(adj.size(), 8);

    // All 8 should be on pitch
    for (auto p : adj) {
        EXPECT_TRUE(p.isOnPitch());
        EXPECT_EQ(center.distanceTo(p), 1);
    }
}

TEST(Position, AdjacentOnPitchCount) {
    // Center of pitch: 8 neighbors
    EXPECT_EQ((Position{10, 7}).adjacentOnPitchCount(), 8);
    // Corner: 3 neighbors
    EXPECT_EQ((Position{0, 0}).adjacentOnPitchCount(), 3);
    // Edge: 5 neighbors
    EXPECT_EQ((Position{0, 7}).adjacentOnPitchCount(), 5);
}

TEST(Position, Equality) {
    EXPECT_EQ((Position{3, 4}), (Position{3, 4}));
    EXPECT_NE((Position{3, 4}), (Position{3, 5}));
    EXPECT_NE((Position{3, 4}), (Position{4, 4}));
}
