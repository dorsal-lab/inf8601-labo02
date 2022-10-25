/** \file matchers.cpp
 * \brief Clang AST Matchers for automatic correction :: implementation
 *
 * \author Sébastien Darche <sebastien.darche@polymtl.ca>
 */

#include <algorithm>
#include <array>

#include "matchers.hpp"

using namespace clang;
using namespace clang::ast_matchers;

// AST matchers

clang::ast_matchers::DeclarationMatcher kernelMatcher =
    functionDecl(hasName("sinoscope_kernel")).bind("kernelDecl");

inline void assertB(bool cond, const std::string& mess) {
    if (!cond) {
        throw std::runtime_error("OpenCL : Failed assertion, " + mess);
    }
}

void KernelCallback::assertVariant(const OclImpl& requirements) const {
    if (!has_matched) {
        throw std::runtime_error("Compilation failed");
    }

    if (args.front() != ArgType::Pointer) {
        llvm::errs() << "Votre énoncé requiert que le buffer partagé soit "
                        "passé en premier paramètre\n";
    }

    switch (std::get<0>(requirements)) {
    case OclParam::IntStructFloatStruct:
        assertB(args.size() == 3, "3 arguments attendus");
        assertB(args[1] == ArgType::Aggregate && args[2] == ArgType::Aggregate,
                "deux structures attendues");
        break;
    case OclParam::OneByOne:
        assertB(std::all_of(args.cbegin() + 1, args.cend(),
                            [](ArgType p) {
                                return p == ArgType::Floating ||
                                       p == ArgType::Integral;
                            }),
                "tous les arguments doivent être passés un à un");
    case OclParam::Struct:
        assertB(args[1] == ArgType::Aggregate,
                "tous les arguments doivent être passés dans une struct");

    case OclParam::IntStructFloatOneByOne:
        assertB(std::all_of(args.cbegin() + 2, args.cend(),
                            [](ArgType p) { return p == ArgType::Integral; }) &&
                    args[1] == ArgType::Aggregate,
                "les entiers doivent être passés en struct et les flottants un "
                "par un");
    case OclParam::FloatStructIntOneByOne:
        assertB(std::all_of(args.cbegin() + 2, args.cend(),
                            [](ArgType p) { return p == ArgType::Floating; }) &&
                    args[1] == ArgType::Aggregate,
                "les flottants doivent être passés en struct et les entiers un "
                "par un");
    }
}

void KernelCallback::run(
    const clang::ast_matchers::MatchFinder::MatchResult& result) {
    if (const auto* match =
            result.Nodes.getNodeAs<FunctionDecl>("kernelDecl")) {
        // match->dump();

        has_matched = true;

        for (auto* parameter : match->parameters()) {
            // parameter->dump();
            auto type = parameter->getOriginalType();

            if (type->isFloatingType()) {
                args.push_back(ArgType::Floating);
            } else if (type->isIntegralType(*result.Context)) {
                args.push_back(ArgType::Integral);
            } else if (type->isAggregateType()) {
                args.push_back(ArgType::Aggregate);
            } else if (type->isPointerType()) {
                args.push_back(ArgType::Pointer);
            } else {
                throw std::runtime_error("Unhandled type");
            }
        }
    }
}
