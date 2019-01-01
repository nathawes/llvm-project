; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -instcombine -S | FileCheck %s

; 'Negate' is considered less complex than a normal binop, so the mul should have the binop as the first operand.

define i8 @neg(i8 %x) {
; CHECK-LABEL: @neg(
; CHECK-NEXT:    [[BO:%.*]] = udiv i8 [[X:%.*]], 42
; CHECK-NEXT:    [[NEGX:%.*]] = sub i8 0, [[X]]
; CHECK-NEXT:    [[R:%.*]] = xor i8 [[BO]], [[NEGX]]
; CHECK-NEXT:    ret i8 [[R]]
;
  %bo = udiv i8 %x, 42
  %negx = sub i8 0, %x
  %r = xor i8 %negx, %bo
  ret i8 %r
}

define <2 x i8> @neg_vec(<2 x i8> %x) {
; CHECK-LABEL: @neg_vec(
; CHECK-NEXT:    [[BO:%.*]] = udiv <2 x i8> [[X:%.*]], <i8 42, i8 -42>
; CHECK-NEXT:    [[NEGX:%.*]] = sub <2 x i8> zeroinitializer, [[X]]
; CHECK-NEXT:    [[R:%.*]] = xor <2 x i8> [[BO]], [[NEGX]]
; CHECK-NEXT:    ret <2 x i8> [[R]]
;
  %bo = udiv <2 x i8> %x, <i8 42, i8 -42>
  %negx = sub <2 x i8> <i8 0, i8 0>, %x
  %r = xor <2 x i8> %negx, %bo
  ret <2 x i8> %r
}

define <2 x i8> @neg_vec_undef(<2 x i8> %x) {
; CHECK-LABEL: @neg_vec_undef(
; CHECK-NEXT:    [[BO:%.*]] = udiv <2 x i8> [[X:%.*]], <i8 42, i8 -42>
; CHECK-NEXT:    [[NEGX:%.*]] = sub <2 x i8> <i8 0, i8 undef>, [[X]]
; CHECK-NEXT:    [[R:%.*]] = xor <2 x i8> [[BO]], [[NEGX]]
; CHECK-NEXT:    ret <2 x i8> [[R]]
;
  %bo = udiv <2 x i8> %x, <i8 42, i8 -42>
  %negx = sub <2 x i8> <i8 0, i8 undef>, %x
  %r = xor <2 x i8> %negx, %bo
  ret <2 x i8> %r
}

; 'Not' is considered less complex than a normal binop, so the mul should have the binop as the first operand.

define i8 @not(i8 %x) {
; CHECK-LABEL: @not(
; CHECK-NEXT:    [[BO:%.*]] = udiv i8 [[X:%.*]], 42
; CHECK-NEXT:    [[NOTX:%.*]] = xor i8 [[X]], -1
; CHECK-NEXT:    [[R:%.*]] = mul i8 [[BO]], [[NOTX]]
; CHECK-NEXT:    ret i8 [[R]]
;
  %bo = udiv i8 %x, 42
  %notx = xor i8 -1, %x
  %r = mul i8 %notx, %bo
  ret i8 %r
}

define <2 x i8> @not_vec(<2 x i8> %x) {
; CHECK-LABEL: @not_vec(
; CHECK-NEXT:    [[BO:%.*]] = udiv <2 x i8> [[X:%.*]], <i8 42, i8 -42>
; CHECK-NEXT:    [[NOTX:%.*]] = xor <2 x i8> [[X]], <i8 -1, i8 -1>
; CHECK-NEXT:    [[R:%.*]] = mul <2 x i8> [[BO]], [[NOTX]]
; CHECK-NEXT:    ret <2 x i8> [[R]]
;
  %bo = udiv <2 x i8> %x, <i8 42, i8 -42>
  %notx = xor <2 x i8> <i8 -1, i8 -1>, %x
  %r = mul <2 x i8> %notx, %bo
  ret <2 x i8> %r
}

define <2 x i8> @not_vec_undef(<2 x i8> %x) {
; CHECK-LABEL: @not_vec_undef(
; CHECK-NEXT:    [[BO:%.*]] = udiv <2 x i8> [[X:%.*]], <i8 42, i8 -42>
; CHECK-NEXT:    [[NOTX:%.*]] = xor <2 x i8> [[X]], <i8 -1, i8 undef>
; CHECK-NEXT:    [[R:%.*]] = mul <2 x i8> [[BO]], [[NOTX]]
; CHECK-NEXT:    ret <2 x i8> [[R]]
;
  %bo = udiv <2 x i8> %x, <i8 42, i8 -42>
  %notx = xor <2 x i8> <i8 -1, i8 undef>, %x
  %r = mul <2 x i8> %notx, %bo
  ret <2 x i8> %r
}

; 'Fneg' is considered less complex than a normal binop, so the fmul should have the binop as the first operand.
; Extra uses are required to ensure that the fneg is not canonicalized after the fmul.

declare void @use(float)
declare void @use_vec(<2 x float>)

define float @fneg(float %x) {
; CHECK-LABEL: @fneg(
; CHECK-NEXT:    [[BO:%.*]] = fdiv float [[X:%.*]], 4.200000e+01
; CHECK-NEXT:    [[FNEGX:%.*]] = fsub float -0.000000e+00, [[X]]
; CHECK-NEXT:    [[R:%.*]] = fmul float [[BO]], [[FNEGX]]
; CHECK-NEXT:    call void @use(float [[FNEGX]])
; CHECK-NEXT:    ret float [[R]]
;
  %bo = fdiv float %x, 42.0
  %fnegx = fsub float -0.0, %x
  %r = fmul float %fnegx, %bo
  call void @use(float %fnegx)
  ret float %r
}

define <2 x float> @fneg_vec(<2 x float> %x) {
; CHECK-LABEL: @fneg_vec(
; CHECK-NEXT:    [[BO:%.*]] = fdiv <2 x float> [[X:%.*]], <float 4.200000e+01, float -4.200000e+01>
; CHECK-NEXT:    [[FNEGX:%.*]] = fsub <2 x float> <float -0.000000e+00, float -0.000000e+00>, [[X]]
; CHECK-NEXT:    [[R:%.*]] = fmul <2 x float> [[BO]], [[FNEGX]]
; CHECK-NEXT:    call void @use_vec(<2 x float> [[FNEGX]])
; CHECK-NEXT:    ret <2 x float> [[R]]
;
  %bo = fdiv <2 x float> %x, <float 42.0, float -42.0>
  %fnegx = fsub <2 x float> <float -0.0, float -0.0>, %x
  %r = fmul <2 x float> %fnegx, %bo
  call void @use_vec(<2 x float> %fnegx)
  ret <2 x float> %r
}

define <2 x float> @fneg_vec_undef(<2 x float> %x) {
; CHECK-LABEL: @fneg_vec_undef(
; CHECK-NEXT:    [[BO:%.*]] = fdiv <2 x float> [[X:%.*]], <float 4.200000e+01, float -4.200000e+01>
; CHECK-NEXT:    [[FNEGX:%.*]] = fsub <2 x float> <float -0.000000e+00, float undef>, [[X]]
; CHECK-NEXT:    [[R:%.*]] = fmul <2 x float> [[BO]], [[FNEGX]]
; CHECK-NEXT:    call void @use_vec(<2 x float> [[FNEGX]])
; CHECK-NEXT:    ret <2 x float> [[R]]
;
  %bo = fdiv <2 x float> %x, <float 42.0, float -42.0>
  %fnegx = fsub <2 x float> <float -0.0, float undef>, %x
  %r = fmul <2 x float> %fnegx, %bo
  call void @use_vec(<2 x float> %fnegx)
  ret <2 x float> %r
}

