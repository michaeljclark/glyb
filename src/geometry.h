/*
 * Rectangle Intersection (2D)
 *
 * Copyright (c) 2020 Michael Clark <michaeljclark@mac.com>
 *
 * License: GPLv3
 *
 * Perform 2D rectangle intersection test and returns intersection
 * topology classes including which axes overlap and the proximal
 * direction of the rectangles relative to each other when they are
 * not overlapping. The interface for the intersect function is:
 *
 *   - intersect_2d intersect(rect_2d r1, rect_2d r2);
 *
 * The rectangle structure contains two points {x, y} relative to
 * origin with 'x' increasing from left to right and 'y' increasing
 * fromn top to bottom.
 *
 *   - struct rect_2d { vec2 p0, p1; };
 *
 * The intersect_2d topology classes are coded using 5 bits:
 *
 *   - { inner, north, south, east, west }
 *
 * The intersect function returns combinations of these bits forming
 * 24 distinct classes, with coding efficiency of 91.7%
 *
 *   D = log₂(|classes|)/bits
 *     = log₂(24)/5
 *     = 4.585/5
 *     = 0.917
 *
 * The algorithm performs 8 less than comparisons {r1,r2}·{p0,p1}·{x,y}
 * from which it decides the class. Insideness is tested with not less
 * than p0 and less than p1, so coordinates may need to be biased with
 * an appropriate epsilon value. 𝛆 = {0.5, 0.5}.
 */

#pragma once

//* Rectangle (2D) *//
struct rect_2d
{
    glm::vec2 p0, p1;
};

//* Intersection topology classes *//
enum intersect_2d
{
    /*
     * Outside cases (0-axis crossing)
     *
     *  ░░░░░ ░░░░░ ░░░░░ █░░░░ ░░█░░ ░░░░█ ░░░░░ ░░░░░
     *  ░┏━┓░ ░┏━┓░ ░┏━┓░ ░┏━┓░ ░┏━┓░ ░┏━┓░ ░┏━┓░ ░┏━┓░
     *  ░┃░┃░ ░┃░┃░ █┃░┃░ ░┃░┃░ ░┃░┃░ ░┃░┃░ ░┃░┃█ ░┃░┃░
     *  ░┗━┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░
     *  ░░█░░ █░░░░ ░░░░░ ░░░░░ ░░░░░ ░░░░░ ░░░░░ ░░░░█
     *
     * Fully inside case (0-axis crossing):
     *
     *  ░░░░░
     *  ░┏━┓░
     *  ░┃█┃░
     *  ░┗━┛░
     *  ░░░░░
     *
     * Overlap cases (1-axis crossing):
     *
     *  ░░░░░ ░░░░░ ░░█░░ ░░░░░
     *  ░┏━┓░ ░┏━┓░ ░┏█┓░ ░┏━┓░
     *  ░┃░┃░ ██░┃░ ░┃░┃░ ░┃░██
     *  ░┗█┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░
     *  ░░█░░ ░░░░░ ░░░░░ ░░░░░
     *
     * Overlap cases (2-axis crossing):
     *
     *  ░░░░░ ██░░░ ░░░██ ░░░░░ ░░█░░ ░░░░░
     *  ░┏━┓░ ██━┓░ ░┏━██ ░┏━┓░ ░┏█┓░ ░┏━┓░
     *  ░┃░┃░ ░┃░┃░ ░┃░┃░ ░┃░┃░ ░┃█┃░ █████
     *  ██━┛░ ░┗━┛░ ░┗━┛░ ░┗━██ ░┗█┛░ ░┗━┛░
     *  ██░░░ ░░░░░ ░░░░░ ░░░██ ░░█░░ ░░░░░
     *
     * Overlap cases (3-axis crossing):
     *
     *  ██░░░ █████ ░░░██ ░░░░░
     *  ██━┓░ █████ ░┏━██ ░┏━┓░
     *  ██░┃░ ░┃░┃░ ░┃░██ ░┃░┃░
     *  ██━┛░ ░┗━┛░ ░┗━██ █████
     *  ██░░░ ░░░░░ ░░░██ █████
     *
     * Fully surrounded case (4-axis crossing):
     *
     *  █████
     *  █┏━┓█
     *  █┃█┃█
     *  █┗━┛█
     *  █████
     */

    none               = 0,

    inner              = (1 << 1),
    north              = (1 << 2),
    east               = (1 << 3),
    south              = (1 << 4),
    west               = (1 << 5),

    north_east         = north | east,
    north_west         = north | west,
    south_east         = south | east,
    south_west         = south | west,

    north_south        = north | south,
    east_west          = east  | west,

    left               = south | west  | north,
    top                = west  | north | east,
    right              = north | east  | south,
    bottom             = east  | south | west,
    surrounded         = north | east  | south | west,

    inner_north        = inner | north,
    inner_north_east   = inner | north | east,
    inner_east         = inner | east,
    inner_south_east   = inner | south | east,
    inner_south        = inner | south,
    inner_south_west   = inner | south | west,
    inner_west         = inner | west,
    inner_north_west   = inner | north | west,

    inner_north_south  = inner | north | south,
    inner_east_west    = inner | east  | west,

    inner_left         = inner | south | west  | north,
    inner_top          = inner | west  | north | east,
    inner_right        = inner | north | east  | south,
    inner_bottom       = inner | east  | south | west,
    inner_surrounded   = inner | north | east  | south | west,
};

intersect_2d intersect(rect_2d r1, rect_2d r2)
{
    int r1_p0_x_lt_r2_p0_x = r1.p0.x < r2.p0.x;
    int r1_p0_x_lt_r2_p1_x = r1.p0.x < r2.p1.x;
    int r1_p1_x_lt_r2_p0_x = r1.p1.x < r2.p0.x;
    int r1_p1_x_lt_r2_p1_x = r1.p1.x < r2.p1.x;
    int r1_p0_y_lt_r2_p0_y = r1.p0.y < r2.p0.y;
    int r1_p0_y_lt_r2_p1_y = r1.p0.y < r2.p1.y;
    int r1_p1_y_lt_r2_p0_y = r1.p1.y < r2.p0.y;
    int r1_p1_y_lt_r2_p1_y = r1.p1.y < r2.p1.y;

    int p0_x_lt = r1_p0_x_lt_r2_p0_x && r1_p0_x_lt_r2_p1_x;
    int p0_x_in = !r1_p0_x_lt_r2_p0_x && r1_p0_x_lt_r2_p1_x;
    int p0_x_ge = !r1_p0_x_lt_r2_p0_x && !r1_p0_x_lt_r2_p1_x;

    int p1_x_lt = r1_p1_x_lt_r2_p0_x && r1_p1_x_lt_r2_p1_x;
    int p1_x_in = !r1_p1_x_lt_r2_p0_x && r1_p1_x_lt_r2_p1_x;
    int p1_x_ge = !r1_p1_x_lt_r2_p0_x && !r1_p1_x_lt_r2_p1_x;

    int p0_y_lt = r1_p0_y_lt_r2_p0_y && r1_p0_y_lt_r2_p1_y;
    int p0_y_in = !r1_p0_y_lt_r2_p0_y && r1_p0_y_lt_r2_p1_y;
    int p0_y_ge = !r1_p0_y_lt_r2_p0_y && !r1_p0_y_lt_r2_p1_y;

    int p1_y_lt = r1_p1_y_lt_r2_p0_y && r1_p1_y_lt_r2_p1_y;
    int p1_y_in = !r1_p1_y_lt_r2_p0_y && r1_p1_y_lt_r2_p1_y;
    int p1_y_ge = !r1_p1_y_lt_r2_p0_y && !r1_p1_y_lt_r2_p1_y;

    /* Outside cases:
     *
     *  ░░░░░ ░░░░░ ░░░░░ █░░░░ ░░█░░ ░░░░█ ░░░░░ ░░░░░
     *  ░┏━┓░ ░┏━┓░ ░┏━┓░ ░┏━┓░ ░┏━┓░ ░┏━┓░ ░┏━┓░ ░┏━┓░
     *  ░┃░┃░ ░┃░┃░ █┃░┃░ ░┃░┃░ ░┃░┃░ ░┃░┃░ ░┃░┃█ ░┃░┃░
     *  ░┗━┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░
     *  ░░█░░ █░░░░ ░░░░░ ░░░░░ ░░░░░ ░░░░░ ░░░░░ ░░░░█
     */
    unsigned out = 0;
    out |= (p0_y_lt && p1_y_lt) ? intersect_2d::north : 0;
    out |= (p0_x_ge && p1_x_ge) ? intersect_2d::east  : 0;
    out |= (p0_y_ge && p1_y_ge) ? intersect_2d::south : 0;
    out |= (p0_x_lt && p1_x_lt) ? intersect_2d::west  : 0;
    if (out) return (intersect_2d)out;

    /* Fully inside case (0-axis crossing):
     *
     *  ░░░░░
     *  ░┏━┓░
     *  ░┃█┃░
     *  ░┗━┛░
     *  ░░░░░
     */
    if (p0_x_in && p1_x_in && p0_y_in && p1_y_in) return intersect_2d::inner;

    /* Overlap cases (1-axis crossing):
     *
     *  ░░░░░ ░░░░░ ░░█░░ ░░░░░
     *  ░┏━┓░ ░┏━┓░ ░┏█┓░ ░┏━┓░
     *  ░┃░┃░ ██░┃░ ░┃░┃░ ░┃░██
     *  ░┗█┛░ ░┗━┛░ ░┗━┛░ ░┗━┛░
     *  ░░█░░ ░░░░░ ░░░░░ ░░░░░
     */
    else if (p0_x_in && p1_x_in && p0_y_in && p1_y_ge) return intersect_2d::inner_south;
    else if (p0_x_lt && p1_x_in && p0_y_in && p1_y_in) return intersect_2d::inner_west;
    else if (p0_x_in && p1_x_in && p0_y_lt && p1_y_in) return intersect_2d::inner_north;
    else if (p0_x_in && p1_x_ge && p0_y_in && p1_y_in) return intersect_2d::inner_east;

    /* Overlap cases (2-axis crossing):
     *
     *  ░░░░░ ██░░░ ░░░██ ░░░░░ ░░█░░ ░░░░░
     *  ░┏━┓░ ██━┓░ ░┏━██ ░┏━┓░ ░┏█┓░ ░┏━┓░
     *  ░┃░┃░ ░┃░┃░ ░┃░┃░ ░┃░┃░ ░┃█┃░ █████
     *  ██━┛░ ░┗━┛░ ░┗━┛░ ░┗━██ ░┗█┛░ ░┗━┛░
     *  ██░░░ ░░░░░ ░░░░░ ░░░██ ░░█░░ ░░░░░
     */
    else if (p0_x_lt && p1_x_in && p0_y_in && p1_y_ge) return intersect_2d::inner_south_west;
    else if (p0_x_lt && p1_x_in && p0_y_lt && p1_y_in) return intersect_2d::inner_north_west;
    else if (p0_x_in && p1_x_ge && p0_y_lt && p1_y_in) return intersect_2d::inner_north_east;
    else if (p0_x_in && p1_x_ge && p0_y_in && p1_y_ge) return intersect_2d::inner_south_east;
    else if (p0_x_in && p1_x_in && p0_y_lt && p1_y_ge) return intersect_2d::inner_north_south;
    else if (p0_x_lt && p1_x_ge && p0_y_in && p1_y_in) return intersect_2d::inner_east_west;

    /* Overlap cases (3-axis crossing):
     *
     *  ██░░░ █████ ░░░██ ░░░░░
     *  ██━┓░ █████ ░┏━██ ░┏━┓░
     *  ██░┃░ ░┃░┃░ ░┃░██ ░┃░┃░
     *  ██━┛░ ░┗━┛░ ░┗━██ █████
     *  ██░░░ ░░░░░ ░░░██ █████
     */
    else if (p0_x_lt && p1_x_in && p0_y_lt && p1_y_ge) return intersect_2d::inner_left;
    else if (p0_x_lt && p1_x_ge && p0_y_lt && p1_y_in) return intersect_2d::inner_top;
    else if (p0_x_in && p1_x_ge && p0_y_lt && p1_y_ge) return intersect_2d::inner_right;
    else if (p0_x_lt && p1_x_ge && p0_y_in && p1_y_ge) return intersect_2d::inner_bottom;

    /* Fully surrounded case (4-axis crossing):
     *
     *   █████
     *   █┏━┓█
     *   █┃█┃█
     *   █┗━┛█
     *   █████
     */
    else if (p0_x_lt && p1_x_ge && p0_y_lt && p1_y_ge) return intersect_2d::inner_surrounded;
    else return intersect_2d::none;
}
