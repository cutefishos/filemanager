// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "testutil.h"

#include "core/standardfolders.h"
#include "model/pathbarmodel.h"
#include "model/places.h"

#include "base/url.h"

#include <gtest/gtest.h>

#include <filesystem>

using fm::PlacesModel;
using fmtest::TemporaryDir;

TEST(PlacesModelTest, FavoritesAddMoveRemoveAndPersist)
{
    TemporaryDir config;
    TemporaryDir folders;
    ASSERT_TRUE(config.isValid() && folders.isValid());
    const std::string settings = config.filePath("filemanager.conf");
    ASSERT_TRUE(fmtest::writeFile(settings, "[Sidebar]\nfavorites=\n"));
    std::filesystem::create_directories(folders.filePath("first"));
    std::filesystem::create_directories(folders.filePath("second"));
    const std::string first = cutefish::base::url::fromLocalFile(folders.filePath("first"));
    const std::string second = cutefish::base::url::fromLocalFile(folders.filePath("second"));

    const auto checkTrash = [](const PlacesModel& places) {
        const PlacesModel::Place& trash = places.at(places.favoriteCount());
        EXPECT_EQ(trash.url, "trash:///");
        EXPECT_TRUE(trash.category.empty());
        EXPECT_FALSE(trash.isDevice());
    };

    PlacesModel model(false, settings);
    EXPECT_EQ(model.favoriteCount(), 0);
    checkTrash(model);
    EXPECT_FALSE(model.canAddFavorites({}));
    EXPECT_FALSE(model.addFavorites({"https://example.com"}, 0));
    EXPECT_FALSE(model.addFavorites({first, cutefish::base::url::fromLocalFile(folders.filePath("missing"))}, 0));
    EXPECT_EQ(model.favoriteCount(), 0);
    EXPECT_TRUE(model.addFavorites({first, second}, 0));
    EXPECT_EQ(model.favoriteCount(), 2);
    checkTrash(model);
    // Already there: nothing is added twice.
    EXPECT_TRUE(model.addFavorites({first}, 0));
    EXPECT_EQ(model.favoriteCount(), 2);

    EXPECT_FALSE(model.moveFavorite(2, 0));
    EXPECT_FALSE(model.moveFavorite(0, 3));
    EXPECT_TRUE(model.moveFavorite(0, 2));
    EXPECT_EQ(model.at(0).url, second);
    EXPECT_TRUE(model.moveFavorite(1, 0));
    EXPECT_EQ(model.at(0).url, first);
    EXPECT_TRUE(model.moveFavorite(0, 1));
    model.removeFavorite(0);
    EXPECT_EQ(model.favoriteCount(), 1);
    // Removing a favourite leaves the folder alone.
    EXPECT_TRUE(std::filesystem::is_directory(folders.filePath("first")));

    {
        PlacesModel restored(false, settings);
        EXPECT_EQ(restored.favoriteCount(), 1);
        checkTrash(restored);
        EXPECT_EQ(restored.at(0).url, second);
    }
    model.removeFavorite(0);
    PlacesModel empty(false, settings);
    EXPECT_EQ(empty.favoriteCount(), 0);
    checkTrash(empty);
}

TEST(PlacesModelTest, PathCrumbsNameHomeAndStandardFolders)
{
    const std::string home = fm::homePath();
    const std::vector<fm::Crumb> crumbs = fm::pathCrumbs(cutefish::base::url::fromLocalFile(home + "/a/b"));
    ASSERT_GE(crumbs.size(), 3u);
    EXPECT_EQ(crumbs.front().name, "Home");
    EXPECT_EQ(crumbs[crumbs.size() - 2].name, "a");
    EXPECT_EQ(crumbs.back().name, "b");
    EXPECT_EQ(fm::standardFolderName(home), "Home");
    EXPECT_TRUE(fm::standardFolderName(home + "/surely-not-a-standard-folder").empty());
}
