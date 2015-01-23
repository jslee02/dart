/*
 * Copyright (c) 2015, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author(s): Jeongseok Lee <jslee02@gmail.com>
 *
 * Georgia Tech Graphics Lab and Humanoid Robotics Lab
 *
 * Directed by Prof. C. Karen Liu and Prof. Mike Stilman
 * <karenliu@cc.gatech.edu> <mstilman@cc.gatech.edu>
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#include "dart/collision/dart/DARTCollideCCD.h"

#include <memory>

namespace dart {
namespace collision {



//==============================================================================
static void shapeToGJK(const CollisionGeometry& s,
                       const Eigen::Isometry3d& tf,
                       ccd_obj_t* o)
{
  const Eigen::Quaterniond dt_rot(tf.rotation());
  const Eigen::Vector3d& dt_pos = tf.translation();

  ccdVec3Set(&o->pos, dt_pos[0], dt_pos[1], dt_pos[2]);
  ccdQuatSet(&o->rot, dt_rot.x(), dt_rot.y(), dt_rot.z(), dt_rot.w());

  ccdQuatInvert2(&o->rot_inv, &o->rot);
}

//==============================================================================
static void boxToGJK(const Box& s,
                     const Eigen::Isometry3d& tf,
                     ccd_box_t* box)
{
  shapeToGJK(s, tf, box);
  box->dim[0] = s.size[0];
  box->dim[1] = s.size[1];
  box->dim[2] = s.size[2];
}

//==============================================================================
static void capToGJK(const Capsule& s,
                     const Eigen::Isometry3d& tf,
                     ccd_cap_t* cap)
{
  shapeToGJK(s, tf, cap);
  cap->radius = s.radius;
  cap->height = s.height;
}

//==============================================================================
static void cylToGJK(const Cylinder& s,
                     const Eigen::Isometry3d& tf,
                     ccd_cyl_t* cyl)
{
  shapeToGJK(s, tf, cyl);
  cyl->radius = s.radius;
  cyl->height = s.height;
}

//==============================================================================
static void coneToGJK(const Cone& s,
                      const Eigen::Isometry3d& tf,
                      ccd_cone_t* cone)
{
  shapeToGJK(s, tf, cone);
  cone->radius = s.radius;
  cone->height = s.height;
}

//==============================================================================
static void sphereToGJK(const Sphere& s,
                        const Eigen::Isometry3d& tf,
                        ccd_sphere_t* sph)
{
  shapeToGJK(s, tf, sph);
  sph->radius = s.radius;
}

//==============================================================================
static void convexToGJK(const Convex& s,
                        const Eigen::Isometry3d& tf,
                        ccd_convex_t* conv)
{
  shapeToGJK(s, tf, conv);
  conv->convex = &s;
}

//==============================================================================
template <>
void* createGJKObject<Sphere>(const Sphere& s, const Eigen::Isometry3d& tf)
{
  std::cout << "createGJKObject<Sphere>()" << std::endl;

  ccd_sphere_t* o = new ccd_sphere_t;
  sphereToGJK(s, tf, o);
  return o;
}

//==============================================================================
template <>
void* createGJKObject<Convex>(const Convex& s, const Eigen::Isometry3d& tf)
{
  ccd_convex_t* o = new ccd_convex_t;
  convexToGJK(s, tf, o);
  return o;
}

/** Support functions */
//==============================================================================
static void supportBox(const void* obj, const ccd_vec3_t* dir_, ccd_vec3_t* v)
{
  const ccd_box_t* o = static_cast<const ccd_box_t*>(obj);
  ccd_vec3_t dir;
  ccdVec3Copy(&dir, dir_);
  ccdQuatRotVec(&dir, &o->rot_inv);
  ccdVec3Set(v, ccdSign(ccdVec3X(&dir)) * o->dim[0],
                ccdSign(ccdVec3Y(&dir)) * o->dim[1],
                ccdSign(ccdVec3Z(&dir)) * o->dim[2]);
  ccdQuatRotVec(v, &o->rot);
  ccdVec3Add(v, &o->pos);
}

//==============================================================================
static void supportCap(const void* obj, const ccd_vec3_t* dir_, ccd_vec3_t* v)
{
  const ccd_cap_t* o = static_cast<const ccd_cap_t*>(obj);
  ccd_vec3_t dir, pos1, pos2;

  ccdVec3Copy(&dir, dir_);
  ccdQuatRotVec(&dir, &o->rot_inv);

  ccdVec3Set(&pos1, CCD_ZERO, CCD_ZERO, o->height);
  ccdVec3Set(&pos2, CCD_ZERO, CCD_ZERO, -o->height);

  ccdVec3Copy(v, &dir);
  ccdVec3Scale(v, o->radius);
  ccdVec3Add(&pos1, v);
  ccdVec3Add(&pos2, v);

  if(ccdVec3Dot(&dir, &pos1) > ccdVec3Dot(&dir, &pos2))
    ccdVec3Copy(v, &pos1);
  else
    ccdVec3Copy(v, &pos2);

  // transform support vertex
  ccdQuatRotVec(v, &o->rot);
  ccdVec3Add(v, &o->pos);
}

//==============================================================================
static void supportCyl(const void* obj, const ccd_vec3_t* dir_, ccd_vec3_t* v)
{
  const ccd_cyl_t* cyl = static_cast<const ccd_cyl_t*>(obj);
  ccd_vec3_t dir;
  double zdist, rad;

  ccdVec3Copy(&dir, dir_);
  ccdQuatRotVec(&dir, &cyl->rot_inv);

  zdist = dir.v[0] * dir.v[0] + dir.v[1] * dir.v[1];
  zdist = sqrt(zdist);
  if(ccdIsZero(zdist))
    ccdVec3Set(v, 0., 0., ccdSign(ccdVec3Z(&dir)) * cyl->height);
  else
  {
    rad = cyl->radius / zdist;

    ccdVec3Set(v, rad * ccdVec3X(&dir),
                  rad * ccdVec3Y(&dir),
                  ccdSign(ccdVec3Z(&dir)) * cyl->height);
  }

  // transform support vertex
  ccdQuatRotVec(v, &cyl->rot);
  ccdVec3Add(v, &cyl->pos);
}

//==============================================================================
static void supportCone(const void* obj, const ccd_vec3_t* dir_, ccd_vec3_t* v)
{
  const ccd_cone_t* cone = static_cast<const ccd_cone_t*>(obj);
  ccd_vec3_t dir;

  ccdVec3Copy(&dir, dir_);
  ccdQuatRotVec(&dir, &cone->rot_inv);

  double zdist, len, rad;
  zdist = dir.v[0] * dir.v[0] + dir.v[1] * dir.v[1];
  len = zdist + dir.v[2] * dir.v[2];
  zdist = sqrt(zdist);
  len = sqrt(len);

  double sin_a = cone->radius / sqrt(cone->radius * cone->radius + 4 * cone->height * cone->height);

  if(dir.v[2] > len * sin_a)
    ccdVec3Set(v, 0., 0., cone->height);
  else if(zdist > 0)
  {
    rad = cone->radius / zdist;
    ccdVec3Set(v, rad * ccdVec3X(&dir), rad * ccdVec3Y(&dir), -cone->height);
  }
  else
    ccdVec3Set(v, 0, 0, -cone->height);

  // transform support vertex
  ccdQuatRotVec(v, &cone->rot);
  ccdVec3Add(v, &cone->pos);
}

//==============================================================================
void supportSphere(const void* obj, const ccd_vec3_t* dir_, ccd_vec3_t* v)
{
  const ccd_sphere_t* s = static_cast<const ccd_sphere_t*>(obj);
  ccd_vec3_t dir;

  ccdVec3Copy(&dir, dir_);
  ccdQuatRotVec(&dir, &s->rot_inv);

  ccdVec3Copy(v, &dir);
  ccdVec3Scale(v, s->radius);
  ccdVec3Scale(v, CCD_ONE / CCD_SQRT(ccdVec3Len2(&dir)));

  // transform support vertex
  ccdQuatRotVec(v, &s->rot);
  ccdVec3Add(v, &s->pos);
}

//==============================================================================
void supportConvex(const void* obj, const ccd_vec3_t* dir_, ccd_vec3_t* v)
{
  const ccd_convex_t* c = static_cast<const ccd_convex_t*>(obj);
  ccd_vec3_t dir, p;
  ccd_real_t maxdot, dot;
  int i;
  Eigen::Vector3d* curp;
  const Eigen::Vector3d& center = c->convex->center;

  ccdVec3Copy(&dir, dir_);
  ccdQuatRotVec(&dir, &c->rot_inv);

  maxdot = -CCD_REAL_MAX;
  curp = c->convex->points;

  for(i = 0; i < c->convex->num_points; ++i, curp += 1)
  {
    ccdVec3Set(&p, (*curp)[0] - center[0], (*curp)[1] - center[1], (*curp)[2] - center[2]);
    dot = ccdVec3Dot(&dir, &p);
    if(dot > maxdot)
    {
      ccdVec3Set(v, (*curp)[0], (*curp)[1], (*curp)[2]);
      maxdot = dot;
    }
  }

  // transform support vertex
  ccdQuatRotVec(v, &c->rot);
  ccdVec3Add(v, &c->pos);
}

//==============================================================================
static void centerShape(const void* obj, ccd_vec3_t* c)
{
  const ccd_obj_t *o = static_cast<const ccd_obj_t*>(obj);
    ccdVec3Copy(c, &o->pos);
}

//==============================================================================
void centerConvex(const void* obj, ccd_vec3_t* c)
{
  const ccd_convex_t *o = static_cast<const ccd_convex_t*>(obj);
  ccdVec3Set(c, o->convex->center[0], o->convex->center[1], o->convex->center[2]);
  ccdQuatRotVec(c, &o->rot);
  ccdVec3Add(c, &o->pos);
}

//==============================================================================
static void centerTriangle(const void* obj, ccd_vec3_t* c)
{
  const ccd_triangle_t *o = static_cast<const ccd_triangle_t*>(obj);
    ccdVec3Copy(c, &o->c);
    ccdQuatRotVec(c, &o->rot);
    ccdVec3Add(c, &o->pos);
}

//==============================================================================
template <>
GJKSupportFunction getSupportFunction<Box>()
{
  return &supportBox;
}

//==============================================================================
template <>
GJKSupportFunction getSupportFunction<Sphere>()
{
  return &supportSphere;
}

//==============================================================================
template <>
GJKSupportFunction getSupportFunction<Capsule>()
{
  return &supportCap;
}

//==============================================================================
template <>
GJKSupportFunction getSupportFunction<Cone>()
{
  return &supportCone;
}

//==============================================================================
template <>
GJKSupportFunction getSupportFunction<Cylinder>()
{
  return &supportCyl;
}

//==============================================================================
template <>
GJKSupportFunction getSupportFunction<Convex>()
{
  return &supportConvex;
}

//==============================================================================
//template <>
//GJKSupportFunction triGetSupportFunction<Tri>()
//{
//  return &supportTriangle;
//}

//==============================================================================
template <>
GJKCenterFunction getCenterFunction<Box>()
{
  return &centerShape;
}

//==============================================================================
template <>
GJKCenterFunction getCenterFunction<Sphere>()
{
  return &centerShape;
}

//==============================================================================
template <>
GJKCenterFunction getCenterFunction<Capsule>()
{
  return &centerShape;
}

//==============================================================================
template <>
GJKCenterFunction getCenterFunction<Cone>()
{
  return &centerShape;
}

//==============================================================================
template <>
GJKCenterFunction getCenterFunction<Cylinder>()
{
  return &centerShape;
}

//==============================================================================
template <>
GJKCenterFunction getCenterFunction<Convex>()
{
  return &centerConvex;
}

//==============================================================================
//template <>
//GJKCenterFunction getCenterFunction<Tri>()
//{
//  return &centerTriangle;
//}

//==============================================================================
size_t collideSphereSphereLibccd(const Sphere& _geom1,
                                 const Eigen::Isometry3d& _tf1,
                                 const Sphere& _geom2,
                                 const Eigen::Isometry3d& _tf2,
                                 const CollisionOptions& _options,
                                 CollisionResult& _result)
{
  std::cout << "collideConvexConvexLibccd()" << std::endl;

  ccd_convex_t c1, c2;

  void* o1 = createGJKObject(_geom1, _tf1);
  void* o2 = createGJKObject(_geom2, _tf2);

  GJKSupportFunction supp1 = getSupportFunction<Sphere>();
  GJKSupportFunction supp2 = getSupportFunction<Sphere>();

  GJKCenterFunction cen1 = getCenterFunction<Sphere>();
  GJKCenterFunction cen2 = getCenterFunction<Sphere>();

  Eigen::Vector3d pos;
  Eigen::Vector3d normal;
  double penetration;

  bool res = ccdCollide(o1, supp1, cen1,
                        o2, supp2, cen2,
                        500, 1e-6,
                        &pos, &normal, &penetration);


  std::cout << "pos        : " << pos.transpose() << std::endl;
  std::cout << "normal     : " << normal.transpose() << std::endl;
  std::cout << "penetration: " << penetration << std::endl;

  return res;
}

//==============================================================================
size_t collideConvexConvexLibccd(const Convex& _geom1,
                                 const Eigen::Isometry3d& _tf1,
                                 const Convex& _geom2,
                                 const Eigen::Isometry3d& _tf2,
                                 const CollisionOptions& _options,
                                 CollisionResult& _result)
{
  std::cout << "collideConvexConvexLibccd()" << std::endl;

  ccd_convex_t c1, c2;

  void* o1 = createGJKObject(_geom1, _tf1);
  void* o2 = createGJKObject(_geom2, _tf2);

  GJKSupportFunction supp1 = getSupportFunction<Convex>();
  GJKSupportFunction supp2 = getSupportFunction<Convex>();

  GJKCenterFunction cen1 = getCenterFunction<Convex>();
  GJKCenterFunction cen2 = getCenterFunction<Convex>();

  Eigen::Vector3d pos;
  Eigen::Vector3d normal;
  double penetration;

  bool res = ccdCollide(o1, supp1, cen1,
                        o2, supp2, cen2,
                        500, 1e-6,
                        &pos, &normal, &penetration);


  std::cout << "pos        : " << pos.transpose() << std::endl;
  std::cout << "normal     : " << normal.transpose() << std::endl;
  std::cout << "penetration: " << penetration << std::endl;

  return res;
}

//==============================================================================
bool ccdCollide(void* obj1, ccd_support_fn supp1, ccd_center_fn cen1,
                void* obj2, ccd_support_fn supp2, ccd_center_fn cen2,
                size_t max_iteration,
                double tolerance,
                Eigen::Vector3d* _point,
                Eigen::Vector3d* _normal,
                double* penetration)
{
  ccd_t ccd;
  ccd_real_t depth;
  ccd_vec3_t dir, pos;

  CCD_INIT(&ccd);
  ccd.support1 = supp1;
  ccd.support2 = supp2;
  ccd.center1  = cen1;
  ccd.center2  = cen2;
  ccd.max_iterations = max_iteration;
  ccd.mpr_tolerance  = tolerance;

  // Check just if the two objects are colliding, and return with the result.
  if (_point == nullptr)
    return ccdMPRIntersect(obj1, obj2, &ccd) != 0 ? true : false;

  // Get contact point, normal, and penetration if there is collision.
  bool isCollided
      = ccdMPRPenetration(obj1, obj2, &ccd, &depth, &dir, &pos) == 0
        ? true : false;

  // If there is collision, extract contact information
  if (isCollided)
  {
    (*_point)[0] = ccdVec3X(&pos);
    (*_point)[1] = ccdVec3Y(&pos);
    (*_point)[2] = ccdVec3Z(&pos);

    assert(_normal != nullptr);

    // Pointing from object2 to object1
    ccdVec3Scale(&dir, -1.);
    (*_normal)[0] = ccdVec3X(&dir);
    (*_normal)[1] = ccdVec3Y(&dir);
    (*_normal)[2] = ccdVec3Z(&dir);
    // TODO: Need test

    *penetration = depth;

    return true;
  }

  return false;
}

} // namespace collision
} // namespace dart