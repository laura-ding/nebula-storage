/* Copyright (c) 2020 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License,
 * attached with Common Clause Condition 1.0, found in the LICENSES directory.
 */

#ifndef STORAGE_INDEXPOLICYMAKER_H
#define STORAGE_INDEXPOLICYMAKER_H

#include "common/base/Base.h"
#include "common/meta/SchemaManager.h"
#include "common/meta/IndexManager.h"
#include "storage/CommonUtils.h"
#include "storage/BaseProcessor.h"

namespace nebula {
namespace storage {

/**
 * OperatorList used record all scan filters, before create scan policy,
 * need collect all columns and values for prefix string.
 * std::tuple<Column name, Value, Operator>;
 */
using OperatorItem = std::tuple<std::string, VariantType, RelationalExpression::Operator>;

class IndexPolicyMaker {
public:
    virtual ~IndexPolicyMaker() = default;

protected:
    explicit IndexPolicyMaker(meta::SchemaManager *schemaMan,
                              meta::IndexManager* indexMan)
        : schemaMan_(schemaMan)
        , indexMan_(indexMan) {}

    /**
     * Details Entry method of index scan policy preparation, process logic as below :
     *         1, Trigger optimizer to optimize expression nodes
     *         2, Traverse the optimized expression, analyze and confirm PolicyType,
     *            confirm scanning mode.
     * Param  filter : From encoded string of where clause.
     * Return ErrorCode
     **/
    nebula::cpp2::ErrorCode preparePolicy(const std::string &filter);

    /**
     * Details Index scan policy generator. Confirm how to generate the scan policy
     *         through the information generated by policyPrepare. Generate corresponding
     *         execution policy according to PolicyType.
     *         In this method, it is best to use as many index columns as possible.
     **/
    void buildPolicy();

    /**
     * Details Evaluate filter conditions.
     */
    bool exprEval(Getters &getters);

private:
    nebula::cpp2::ErrorCode decodeExpression(const std::string &filter);

    /**
     * Details Entry method of expresion traverse.
     */

    nebula::cpp2::ErrorCode traversalExpression(const Expression *expr);

protected:
    meta::SchemaManager*                     schemaMan_{nullptr};
    meta::IndexManager*                      indexMan_{nullptr};
    std::unique_ptr<ExpressionContext>       expCtx_{nullptr};
    std::unique_ptr<Expression>              exp_{nullptr};
    std::string                              prefix_;
    std::shared_ptr<nebula::cpp2::IndexItem> index_{nullptr};
    bool                                     optimizedPolicy_{true};
    bool                                     requiredFilter_{true};
    std::vector<OperatorItem>                operatorList_;
};
}  // namespace storage
}  // namespace nebula
#endif  // STORAGE_INDEXPOLICYMAKER_H
