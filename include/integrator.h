#pragma once

// Classic 4th-order Runge-Kutta step, generic over the state type.
//
// `State` must support  State + State  and  double * State.
// `f(t, y)` must return dy/dt as a `State`.
//
// It is a template on purpose: the Schwarzschild orbital-plane reduction uses a
// 2-variable state now, and a full Kerr geodesic will use a larger one later.
// Only the state type and the derivative function change — this stepper does not.
template <typename State, typename Derivative>
State rk4_step(const State& y, double t, double h, Derivative f) {
    const State k1 = f(t, y);
    const State k2 = f(t + 0.5 * h, y + (0.5 * h) * k1);
    const State k3 = f(t + 0.5 * h, y + (0.5 * h) * k2);
    const State k4 = f(t + h, y + h * k3);

    return y + (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
}
