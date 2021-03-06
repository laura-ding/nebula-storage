/* Copyright (c) 2020 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License,
 * attached with Common Clause Condition 1.0, found in the LICENSES directory.
 */

#include "storage/admin/FlushTask.h"
#include "common/base/Logging.h"

namespace nebula {
namespace storage {

ErrorOr<cpp2::ErrorCode, std::vector<AdminSubTask>>
FlushTask::genSubTasks() {
    std::vector<AdminSubTask> ret;
    if (!ctx_.store_) {
        return ret;
    }

    auto* store = dynamic_cast<kvstore::NebulaStore*>(ctx_.store_);
    CHECK_NOTNULL(store);
    auto errOrSpace = store->space(ctx_.spaceId_);
    if (!ok(errOrSpace)) {
        return error(errOrSpace);
    }

    auto space = nebula::value(errOrSpace);

    ret.emplace_back([space = space](){
        for (auto& engine : space->engines_) {
            auto code = engine->flush();
            if (code != kvstore::ResultCode::SUCCEEDED) {
                return code;
            }
        }
        return kvstore::ResultCode::SUCCEEDED;
    });
    return ret;
}

}  // namespace storage
}  // namespace nebula
