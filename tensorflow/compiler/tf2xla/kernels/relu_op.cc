/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// Native XLA implementations of XLA Relu Ops

#include "tensorflow/compiler/tf2xla/kernels/cwise_ops.h"
#include "tensorflow/compiler/tf2xla/xla_helpers.h"
#include "tensorflow/compiler/tf2xla/xla_op_registry.h"
#include "tensorflow/compiler/xla/client/computation_builder.h"
#include "tensorflow/compiler/xla/literal_util.h"
#include "tensorflow/core/framework/kernel_def_builder.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/kernels/no_op.h"

namespace tensorflow {
namespace {

class ReluOp : public XlaOpKernel {
 public:
  explicit ReluOp(OpKernelConstruction* ctx) : XlaOpKernel(ctx) {}
  // Computes the max of the scalar input x and 0.
  void Compile(XlaOpKernelContext* ctx) {
    xla::ComputationBuilder* builder = ctx->builder();
    auto zero = XlaHelpers::Zero(builder, input_type(0));
    ctx->SetOutput(0, builder->Max(zero, ctx->Input(0)));
  }
};

class Relu6Op : public XlaOpKernel {
 public:
  explicit Relu6Op(OpKernelConstruction* ctx) : XlaOpKernel(ctx) {}
  // Clamp the scalar input between 0 and 6.
  void Compile(XlaOpKernelContext* ctx) {
    xla::ComputationBuilder* builder = ctx->builder();
    auto zero = XlaHelpers::Zero(builder, input_type(0));
    auto six = XlaHelpers::IntegerLiteral(builder, input_type(0), 6);
    ctx->SetOutput(0, builder->Clamp(zero, ctx->Input(0), six));
  }
};

class Relu1Op : public XlaOpKernel {
public:
    explicit Relu1Op(OpKernelConstruction* ctx) : XlaOpKernel(ctx) {}
    // Clamp the scalar input between 0 and 1.
    void Compile(XlaOpKernelContext* ctx) {
        xla::ComputationBuilder* builder = ctx->builder();
        auto zero = XlaHelpers::Zero(builder, input_type(0));
        auto one = XlaHelpers::IntegerLiteral(builder, input_type(0), 1);
        ctx->SetOutput(0, builder->Clamp(zero, ctx->Input(0), one));
    }
};

// A subclass of a XlaBinaryMapOp must build the lambda computation
// that describes the (scalar,scalar)->scalar function to apply to
// each element of the input. We have to use XlaBinaryMapOp instead of
// XlaBinaryOp here because XLA Select does not do automatic
// broadcasting.
class ReluGradOp : public XlaBinaryMapOp {
 public:
  explicit ReluGradOp(OpKernelConstruction* ctx) : XlaOpKernel(ctx) {}
  // Return the lhs (incoming gradient) if the rhs (input feature) > 0,
  // otherwise return 0.
  void Compile(XlaOpKernelContext* ctx) {
    xla::ComputationBuilder* b = ctx->builder();
    const TensorShape shape = ctx->InputShape(0);
    const auto zero =
        b->Broadcast(XlaHelpers::Zero(b, input_type(0)), shape.dim_sizes());
    const auto pred = b->Gt(ctx->Input(1), zero);
    ctx->SetOutput(0, b->Select(pred, ctx->Input(0), zero));
  }
};

class Relu6GradOp : public XlaOpKernel {
 public:
  explicit Relu6GradOp(OpKernelConstruction* ctx) : XlaOpKernel(ctx) {}
  // Return the lhs (incoming gradient) if the rhs (input feature) > 0,
  // otherwise return 0.
  void Compile(XlaOpKernelContext* ctx) {
    xla::ComputationBuilder* b = ctx->builder();
    const TensorShape shape = ctx->InputShape(0);
    const auto zero =
        b->Broadcast(XlaHelpers::Zero(b, input_type(0)), shape.dim_sizes());
    const auto six = b->Broadcast(
        XlaHelpers::IntegerLiteral(b, input_type(0), 6), shape.dim_sizes());
    auto out = b->Select(
        b->LogicalAnd(b->Lt(ctx->Input(1), six), b->Gt(ctx->Input(1), zero)),
        ctx->Input(0), zero);
    ctx->SetOutput(0, out);
  }
};

class Relu1GradOp : public XlaBinaryMapOp {
public:
    explicit Relu1GradOp(OpKernelConstruction* ctx) : XlaBinaryMapOp(ctx) {}
    // Return the lhs (incoming gradient) if the rhs (input feature) > 0,
    // otherwise return 0.
    void BuildMapLambda(xla::ComputationBuilder* b,
        const xla::ComputationDataHandle& gradient,
        const xla::ComputationDataHandle& feature) override {
        const auto zero = XlaHelpers::Zero(b, input_type(0));
        auto one = XlaHelpers::IntegerLiteral(b, input_type(0), 1);
        b->Select(b->LogicalAnd(b->Lt(feature, one), b->Gt(feature, zero)),
            gradient, zero);
    }
};

REGISTER_XLA_OP("Relu", ReluOp);
REGISTER_XLA_OP("Relu6", Relu6Op);
REGISTER_XLA_OP("Relu1", Relu1Op);
REGISTER_XLA_OP("ReluGrad", ReluGradOp);
REGISTER_XLA_OP("Relu6Grad", Relu6GradOp);
REGISTER_XLA_OP("Relu1Grad", Relu1GradOp);

}  // namespace
}  // namespace tensorflow
