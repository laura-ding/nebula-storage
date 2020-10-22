/* Copyright (c) 2020 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License,
 * attached with Common Clause Condition 1.0, found in the LICENSES directory.
 */

#include "common/base/Base.h"
#include "common/fs/FileUtils.h"
#include "tools/meta-data-upgrade/MetaDataUpgrade.h"
#include "tools/meta-data-upgrade/oldThrift/MetaServiceUtilsV1.h"
#include "meta/processors/jobMan/JobDescription.h"

DEFINE_string(meta_data_path, "", "meta data path");
DEFINE_bool(null_type, true, "set schema to support null type");
DEFINE_bool(print_info, false, "enable to print the rewrite data");

int main(int argc, char *argv[]) {
    folly::init(&argc, &argv, true);
    google::SetStderrLogging(google::INFO);

    LOG(INFO) << "Input meta data path: " << FLAGS_meta_data_path;
    if (FLAGS_meta_data_path.empty()) {
        LOG(INFO) << "Please input meta data path: --meta_data_path=<meta data path>";
        return 1;
    }

    if (!nebula::fs::FileUtils::exist(FLAGS_meta_data_path)) {
        LOG(ERROR) << FLAGS_meta_data_path << " is not exist.";
        return 1;
    }

    auto newDataPath = nebula::fs::FileUtils::dirname(FLAGS_meta_data_path.c_str()) + "/v2.0_data";
    LOG(INFO) << "2.0 meta data path: " << newDataPath;
    if (nebula::fs::FileUtils::exist(newDataPath)) {
        LOG(WARNING) << newDataPath << " is existed, remove it.";
        if (!nebula::fs::FileUtils::remove(newDataPath.c_str(), true)) {
            LOG(ERROR) << "Delete " << newDataPath << " failed.";
            return 1;
        }
    }

    if (!nebula::fs::FileUtils::makeDir(newDataPath)) {
        LOG(ERROR) << "makeDir " << newDataPath << " failed";
        return 0;
    }

    // copy the data path to rewrite
    auto copyCmd = folly::stringPrintf("cp -r %s/* %s",
                                       FLAGS_meta_data_path.c_str(),
                                       newDataPath.c_str());
    LOG(INFO) << "Copy cmd: " << copyCmd;
    std::system(copyCmd.c_str());

    auto dataPath = folly::stringPrintf("%s/nebula/0/data", newDataPath.c_str());
    LOG(INFO) << "The new data path is " << dataPath;

    rocksdb::WriteOptions options;
    nebula::meta::MetaDataUpgrade upgrader;
    auto status = upgrader.initDB(dataPath);
    if (!status.ok()) {
        LOG(ERROR) << "InitDB from `" << FLAGS_meta_data_path << "' failed: " << status;
        return 1;
    }
    auto iter = upgrader.getDbIter();
    if (!iter) {
        LOG(ERROR) << "Get nullptr iter from rocksdb.";
        return 1;
    }

    iter->SeekToFirst();
    while (iter->Valid()) {
        auto key = folly::StringPiece(iter->key().data(), iter->key().size());
        auto val = folly::StringPiece(iter->value().data(), iter->value().size());

        if (key.startsWith(nebula::oldmeta::kSpacesTable)) {
            if (FLAGS_print_info) { upgrader.printSpaces(val); }
            status = upgrader.rewriteSpaces(key, val);
        } else if (key.startsWith(nebula::oldmeta::kPartsTable)) {
            if (FLAGS_print_info) { upgrader.printParts(key, val); }
            status = upgrader.rewriteParts(key, val);
        } else if (key.startsWith(nebula::oldmeta::kHostsTable)) {
            if (FLAGS_print_info) { upgrader.printHost(key, val); }
            status = upgrader.rewriteHosts(key, val);
        } else if (key.startsWith(nebula::oldmeta::kLeadersTable)) {
            if (FLAGS_print_info) { upgrader.printLeaders(key); }
            status = upgrader.rewriteLeaders(key, val);
        } else if (key.startsWith(nebula::oldmeta::kTagsTable)
                   || key.startsWith(nebula::oldmeta::kEdgesTable)) {
            if (FLAGS_print_info) { upgrader.printSchemas(val); }
            status = upgrader.rewriteSchemas(key, val);
        } else if (key.startsWith(nebula::oldmeta::kIndexesTable)) {
            if (FLAGS_print_info) { upgrader.printIndexes(val); }
            status = upgrader.rewriteIndexes(key, val);
        } else if (key.startsWith(nebula::oldmeta::kConfigsTable)) {
            if (FLAGS_print_info) { upgrader.printConfigs(key, val); }
            status = upgrader.rewriteConfigs(key, val);
        } else if (nebula::meta::JobDescription::isJobKey(key)) {
            if (FLAGS_print_info) { upgrader.printJobDesc(key, val); }
            status = upgrader.rewriteJobDesc(key, val);
        } else if (key.startsWith(nebula::oldmeta::kDefaultTable) ||
                   key.startsWith(nebula::oldmeta::kCurrJob) ||
                   key.startsWith(nebula::oldmeta::kJobArchive) ||
                   key.startsWith(nebula::oldmeta::kJob)) {
            status = upgrader.deleteKeyVal(key);
        }
        if (!status.ok()) {
            LOG(ERROR) << status;
            // remove the new data directory
            if (!nebula::fs::FileUtils::remove(newDataPath.c_str(), true)) {
                LOG(ERROR) << "Delete " << newDataPath << " failed.";
                return 1;
            }
        }
        iter->Next();
    }
    return 0;
}
