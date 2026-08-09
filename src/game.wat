(module
  ;; Game logic for a square moving on a rectangular baseplate.
  ;; Coordinates are in world units; JS maps them to canvas pixels.

  ;; Baseplate dimensions (world units)
  (global $PLATE_W f32 (f32.const 800))
  (global $PLATE_H f32 (f32.const 500))
  ;; Player square size
  (global $PLAYER_SIZE f32 (f32.const 40))
  ;; Movement tuning
  (global $ACCEL f32 (f32.const 2400))
  (global $MAX_SPEED f32 (f32.const 420))
  (global $FRICTION f32 (f32.const 8))

  ;; Player state
  (global $px (mut f32) (f32.const 380))
  (global $py (mut f32) (f32.const 230))
  (global $vx (mut f32) (f32.const 0))
  (global $vy (mut f32) (f32.const 0))

  (func $clamp (param $v f32) (param $lo f32) (param $hi f32) (result f32)
    (f32.min (f32.max (local.get $v) (local.get $lo)) (local.get $hi))
  )

  ;; Advance simulation by dt seconds with input axes in [-1, 1].
  (func $update (export "update") (param $dt f32) (param $inx f32) (param $iny f32)
    (local $len f32)
    (local $decay f32)

    ;; Normalize diagonal input so it isn't faster
    (local.set $len
      (f32.sqrt
        (f32.add
          (f32.mul (local.get $inx) (local.get $inx))
          (f32.mul (local.get $iny) (local.get $iny)))))
    (if (f32.gt (local.get $len) (f32.const 1))
      (then
        (local.set $inx (f32.div (local.get $inx) (local.get $len)))
        (local.set $iny (f32.div (local.get $iny) (local.get $len)))))

    ;; Accelerate
    (global.set $vx
      (f32.add (global.get $vx)
        (f32.mul (f32.mul (local.get $inx) (global.get $ACCEL)) (local.get $dt))))
    (global.set $vy
      (f32.add (global.get $vy)
        (f32.mul (f32.mul (local.get $iny) (global.get $ACCEL)) (local.get $dt))))

    ;; Exponential friction: v *= 1 / (1 + FRICTION * dt)
    (local.set $decay
      (f32.div (f32.const 1)
        (f32.add (f32.const 1) (f32.mul (global.get $FRICTION) (local.get $dt)))))
    (global.set $vx (f32.mul (global.get $vx) (local.get $decay)))
    (global.set $vy (f32.mul (global.get $vy) (local.get $decay)))

    ;; Clamp speed
    (global.set $vx (call $clamp (global.get $vx)
      (f32.neg (global.get $MAX_SPEED)) (global.get $MAX_SPEED)))
    (global.set $vy (call $clamp (global.get $vy)
      (f32.neg (global.get $MAX_SPEED)) (global.get $MAX_SPEED)))

    ;; Integrate position
    (global.set $px (f32.add (global.get $px) (f32.mul (global.get $vx) (local.get $dt))))
    (global.set $py (f32.add (global.get $py) (f32.mul (global.get $vy) (local.get $dt))))

    ;; Keep the square on the baseplate; kill velocity at the walls
    (if (f32.lt (global.get $px) (f32.const 0))
      (then (global.set $px (f32.const 0)) (global.set $vx (f32.const 0))))
    (if (f32.gt (global.get $px) (f32.sub (global.get $PLATE_W) (global.get $PLAYER_SIZE)))
      (then
        (global.set $px (f32.sub (global.get $PLATE_W) (global.get $PLAYER_SIZE)))
        (global.set $vx (f32.const 0))))
    (if (f32.lt (global.get $py) (f32.const 0))
      (then (global.set $py (f32.const 0)) (global.set $vy (f32.const 0))))
    (if (f32.gt (global.get $py) (f32.sub (global.get $PLATE_H) (global.get $PLAYER_SIZE)))
      (then
        (global.set $py (f32.sub (global.get $PLATE_H) (global.get $PLAYER_SIZE)))
        (global.set $vy (f32.const 0))))
  )

  (func (export "getX") (result f32) (global.get $px))
  (func (export "getY") (result f32) (global.get $py))
  (func (export "getVX") (result f32) (global.get $vx))
  (func (export "getVY") (result f32) (global.get $vy))
  (func (export "getPlateW") (result f32) (global.get $PLATE_W))
  (func (export "getPlateH") (result f32) (global.get $PLATE_H))
  (func (export "getPlayerSize") (result f32) (global.get $PLAYER_SIZE))
)
