// Copyright 2014 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MOTIVE_MOTIVATOR_H
#define MOTIVE_MOTIVATOR_H

#include "motive/processor.h"

namespace motive {

class MotiveEngine;

/// @class Motivator
/// @brief Drives a value towards a target value, or along a path.
///
/// The value can be one-dimensional (e.g. a float), or multi-dimensional
/// (e.g. a matrix). The dimension is determined by the sub-class:
/// Motivator1f drives a float, MotivatorMatrix4f drives a 4x4 float matrix.
///
/// Although you can instandiate a Motivator, you probably will not, since
/// there is no mechanism to read data out of a Motivator. Generally, you
/// will instantiate a derived class like Motivator1f, which provides
/// accessor functions.
///
/// The way a Motivator's value moves towards its target is determined by the
/// **type** of a motivator. The type is specified in Motivator::Initialize().
///
/// Note that a Motivator does not store any data itself. It is a handle into
/// a MotiveProcessor. Each MotiveProcessor holds all data for motivators
/// of its **type**.
///
/// Only one Motivator can reference a specific index in a MotiveProcessor.
/// Therefore, when you copy a Motivator, the original motivator will become
/// invalid.
///
class Motivator {
 public:
  Motivator() : processor_(nullptr), index_(kMotiveIndexInvalid) {}
  Motivator(const MotivatorInit& init, MotiveEngine* engine)
      : processor_(nullptr), index_(kMotiveIndexInvalid) {
    Initialize(init, engine);
  }

  /// Allow Motivators to be copied.
  /// Transfer ownership of motivator to new motivator. Old motivator is reset
  /// and must be initialized again before being read.
  /// We want to allow copies primarily so that we can have vectors of
  /// Motivators.
  Motivator(const Motivator& original) {
    if (original.Valid()) {
      original.processor_->TransferMotivator(original.index_, this);
    } else {
      processor_ = nullptr;
      index_ = kMotiveIndexInvalid;
    }
  }

  /// Allow Motivators to be copied. `original` is reset.
  /// See the copy constructor for details.
  Motivator& operator=(const Motivator& original) {
    Invalidate();
    original.processor_->TransferMotivator(original.index_, this);
    return *this;
  }

  /// Remove ourselves from the MotiveProcessor when we're deleted.
  ~Motivator() { Invalidate(); }

  /// Initialize this Motivator to the type specified in init.type.
  /// @param init Defines the type and initial state of the Motivator.
  /// @param engine The engine that will update this Motivator when
  ///               engine->AdvanceFrame() is called.
  void Initialize(const MotivatorInit& init, MotiveEngine* engine);

  /// Detatch this Motivator from its MotiveProcessor. Functions other than
  /// Initialize() and Valid() can no longer be called afterwards.
  void Invalidate() {
    if (processor_ != nullptr) {
      processor_->RemoveMotivator(index_);
    }
  }

  /// Return true if this Motivator is currently being driven by a
  /// MotiveProcessor. That is, if it has been successfully initialized.
  /// Also check for a consistent internal state.
  bool Valid() const {
    return processor_ != nullptr && processor_->ValidMotivator(index_, this);
  }

  /// Return the type of Motivator we've been initilized to.
  /// A Motivator can take on any type that matches its dimension.
  /// The Motivator's type is determined by the `init` param in Initialize().
  MotivatorType Type() const { return processor_->Type(); }

  /// The number of floats (or doubles) that this Motivator is driving.
  /// For example, if this Motivator is driving a 4x4 matrix,
  /// then we will return 16 here.
  int Dimensions() const { return processor_->Dimensions(); }

 protected:
  /// The MotiveProcessor uses the functions below. It does not modify data
  /// directly.
  friend MotiveProcessor;

  /// These should only be called by MotiveProcessor!
  void Init(MotiveProcessor* processor, MotiveIndex index) {
    processor_ = processor;
    index_ = index;
  }
  void Reset() { Init(nullptr, kMotiveIndexInvalid); }
  const MotiveProcessor* Processor() const { return processor_; }

  /// All calls to an Motivator are proxied to an MotivatorProcessor. Motivator
  /// data and processing is centralized to allow for scalable optimizations
  /// (e.g. SIMD or parallelization).
  MotiveProcessor* processor_;

  /// An MotiveProcessor processes one MotivatorType, and hosts every Motivator
  /// of
  /// that type. The id here uniquely identifies this Motivator to the
  /// MotiveProcessor.
  MotiveIndex index_;
};

/// @class Motivator1f
/// @brief Drive a single float value towards a target, or along a spline.
///
/// The current and target values and velocities can be specified by SetTarget()
/// or SetSpline().
///
class Motivator1f : public Motivator {
 public:
  /// Motivator is created in a reset state. When in the reset state,
  /// it is not being driven, and Value(), Velocity(), etc. cannot be called.
  Motivator1f() {}

  /// Initialize to the type specified by `init`. Current and target values
  /// are not set.
  Motivator1f(const MotivatorInit& init, MotiveEngine* engine)
      : Motivator(init, engine) {}

  /// Initialize to the type specified by `init`. Set current and target values
  /// as specified by `t`.
  Motivator1f(const MotivatorInit& init, MotiveEngine* engine,
              const MotiveTarget1f& t)
      : Motivator(init, engine) {
    SetTarget(t);
  }

  /// Initialize to the type specified by `init`. Set current and target values
  /// as specified by `t`.
  void InitializeWithTarget(const MotivatorInit& init, MotiveEngine* engine,
                            const MotiveTarget1f& t) {
    Initialize(init, engine);
    SetTarget(t);
  }

  /// Returns the current motivator value. The current value is updated when
  /// engine->AdvanceFrame() is called on the `engine` that initialized this
  /// Motivator.
  float Value() const { return Processor().Value(index_); }

  /// Returns the current rate of change of this motivator. For example,
  /// if this Motivator is being driven by a spline, returns the derivative
  /// at the current time in the spline curve.
  float Velocity() const { return Processor().Velocity(index_); }

  /// Returns the value this Motivator is driving towards.
  /// If being driven by a spline, returns the value at the end of the spline.
  float TargetValue() const { return Processor().TargetValue(index_); }

  /// Returns the rate-of-change of this Motivator once it reaches
  /// TargetValue().
  float TargetVelocity() const { return Processor().TargetVelocity(index_); }

  /// Returns TargetValue() minus Value(). If we're driving a
  /// modular type (e.g. an angle), this may not be the naive subtraction.
  /// For example, if TargetValue() = 170 degrees, Value() = -170 degrees,
  /// then Difference() = -20 degrees.
  float Difference() const { return Processor().Difference(index_); }

  /// Returns time remaining until target is reached.
  /// The unit of time is determined by the calling program.
  MotiveTime TargetTime() const { return Processor().TargetTime(index_); }

  /// Set the target and (optionally the current) motivator values.
  /// Use this call to procedurally drive the Motivator towards a specific
  /// target. The Motivator will transition smoothly to the new target.
  /// You can change the target value every frame if you like, and the
  /// Motivator value should behave calmly but responsively, with the
  /// movement qualities of the underlying MotiveProcessor.
  /// Note that the underlying MotiveProcessor is allowed to ignore
  /// parts of `t` that are irrelevent to its algorithm.
  /// @param t A set of waypoints to hit, optionally including the current
  ///          value. If the current value is not included, maintain the
  ///          existing current value.
  void SetTarget(const MotiveTarget1f& t) { Processor().SetTarget(index_, t); }

  /// Follow the curve specified in `s`. Overrides the existing current value.
  /// @param s The spline to follow, the time in that spline to initiate
  ///          playback, and whether to repeat from the beginning after the
  ///          end of the spline is reached.
  void SetSpline(const fpl::SplinePlayback& s) {
    Processor().SetSpline(index_, s);
  }

 private:
  MotiveProcessor1f& Processor() {
    return *static_cast<MotiveProcessor1f*>(processor_);
  }
  const MotiveProcessor1f& Processor() const {
    return *static_cast<const MotiveProcessor1f*>(processor_);
  }
};

/// @class MotivatorMatrix4fTemplate
/// @brief Drive a 4x4 float matrix from a series of basic transformations.
///
/// The underlying basic transformations can be animated with
/// SetChildTarget1f(), and set to fixed values with SetChildValue1f() and
/// SetChildValue3f().
///
/// Internally, we use mathfu::mat4 as our matrix type and mathfu::vec3 as
/// our vector type, but external we allow any matrix type to be specified
/// via the VectorConverter template parameter.
///
template <class VectorConverter>
class MotivatorMatrix4fTemplate : public Motivator {
  typedef VectorConverter C;
  typedef typename VectorConverter::ExternalMatrix4 Mat4;
  typedef typename VectorConverter::ExternalVector3 Vec3;

 public:
  MotivatorMatrix4fTemplate() {}
  MotivatorMatrix4fTemplate(const MotivatorInit& init, MotiveEngine* engine)
      : Motivator(init, engine) {}

  /// Return the current value of the Motivator. The processor returns a
  /// vector-aligned matrix, so the cast should be valid for any user-defined
  /// matrix type.
  const Mat4& Value() const { return C::To(Processor().Value(index_)); }

  /// Return the translation component of the matrix.
  /// The matrix is a 3D affine transform, so the translation component is the
  /// fourth column.
  Vec3 Position() const {
    return C::To(Processor().Value(index_).TranslationVector3D());
  }

  /// Return the current value of the `child_index`th basic transform that
  /// drives this matrix.
  /// @param child_index The index into MatrixInit::ops(). The ops() array
  ///                    is a series of basic transformation operations that
  ///                    compose this matrix. Each basic transformation has
  ///                    a current value. We gather this value here.
  float ChildValue1f(MotiveChildIndex child_index) const {
    return Processor().ChildValue1f(index_, child_index);
  }

  /// Returns the current values of the basic transforms at indices
  /// (child_index, child_index + 1, child_index + 2).
  /// Useful when you drive all the (x,y,z) components of a translation, scale,
  /// or rotation.
  /// @param child_index The first index into MatrixInit::ops(). The value at
  ///                    this index is returned in the x component of Vec3.
  ///                    y gets child_index + 1's value, and
  ///                    z gets child_index + 2's value.
  Vec3 ChildValue3f(MotiveChildIndex child_index) const {
    return C::To(Processor().ChildValue3f(index_, child_index));
  }

  /// Set the target the 'child_index'th basic transform.
  /// Each basic transform can be driven by a child motivator.
  /// This call lets us control those child motivators.
  /// @param child_index The index into the MatrixInit::ops() that was passed
  ///                    into Initialize(). This operation must have been
  ///                    initialized with a MotivatorInit, not a constant value.
  /// @param t The target values for the basic transform to animate towards.
  ///          Also, optionally, the current value for it to jump to.
  void SetChildTarget1f(MotiveChildIndex child_index, const MotiveTarget1f& t) {
    Processor().SetChildTarget1f(index_, child_index, t);
  }

  /// Set the constant value of a child. Each basic matrix transformation
  /// can be driven by a constant value instead of a Motivator.
  /// This call lets us set those constant values.
  /// @param child_index The index into the MatrixInit::ops() that was passed
  ///                    into Initialize(). This operation must have been
  ///                    initialized with a constant value, not a MotivatorInit.
  /// @param value The new constant value of this operation.
  void SetChildValue1f(MotiveChildIndex child_index, float value) {
    Processor().SetChildValue1f(index_, child_index, value);
  }

  /// Set the constant values of the basic transforms at indices
  /// (child_index, child_index + 1, child_index + 2).
  /// @param child_index The first index into MatrixInit::ops(). The constant
  ///                    value at this index is set to the x component of
  ///                    `value`.
  ///                    child_index + 1's constant is set to value.y, and
  ///                    child_index + 2's constant is set to value.z.
  void SetChildValue3f(MotiveChildIndex child_index, const Vec3& value) {
    Processor().SetChildValue3f(index_, child_index, C::From(value));
  }

 private:
  MotiveProcessorMatrix4f& Processor() {
    return *static_cast<MotiveProcessorMatrix4f*>(processor_);
  }
  const MotiveProcessorMatrix4f& Processor() const {
    return *static_cast<const MotiveProcessorMatrix4f*>(processor_);
  }
};

// External types are also mathfu in this converter. Create your own converter
// if you'd like to use your own vector types in MotivatorMatrix4fTemplate's
// external API.
class PassThroughVectorConverter {
 public:
  typedef mathfu::mat4 ExternalMatrix4;
  typedef mathfu::vec3 ExternalVector3;
  static const ExternalMatrix4& To(const mathfu::mat4& m) { return m; }
  static ExternalVector3 To(const mathfu::vec3& v) { return v; }
  static const mathfu::vec3& From(const ExternalVector3& v) { return v; }
};

typedef MotivatorMatrix4fTemplate<PassThroughVectorConverter> MotivatorMatrix4f;

}  // namespace motive

#endif  // MOTIVE_MOTIVATOR_H
