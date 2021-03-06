#include "vis.h"

#include "world.h"
#include <fstream>

#ifdef __APPLE__
//why, apple?   why????
#include <OpenGL/glu.h>
#else
#include <gl/glu.h>
#endif

#include "utils.h"
#include "color_spaces.h"

#include "enumerate.hpp"
using benlib::enumerate;
#include "range.hpp"
using benlib::range;




void drawWorldPretty(const World& world,
	const Camera& camera, 
	const VisSettings& settings, 
	SDL_Window* window){

  glEnable(GL_DEPTH_TEST);
  glFrontFace(GL_CCW);
  glEnable(GL_CULL_FACE);
  glClearColor(0.2, 0.2, 0.2, 1);
  glClear(GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  int windowWidth, windowHeight;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);
  gluPerspective(45,static_cast<double>(windowWidth)/windowHeight,
	  .5, 100);

  
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(camera.position.x(), camera.position.y(), camera.position.z(),
	  camera.lookAt.x(), camera.lookAt.y(), camera.lookAt.z(),
	  camera.up.x(), camera.up.y(), camera.up.z());

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  //avoid typing world.clusters, world.particles everywhere
  const auto& clusters = world.clusters;
  const auto& particles = world.particles;
  const int which_cluster = settings.which_cluster;

  if(!particles.empty()){						
	glPointSize(10);

	if (!settings.drawColoredParticles) {
	  if (settings.which_cluster == -1) {
		glColor4d(1,1,1,0.95);
	  } else {
		glColor4d(1,1,1,0.2);
	  }
	  glEnableClientState(GL_VERTEX_ARRAY);
	  glVertexPointer(3, GL_DOUBLE,
		  sizeof(Particle),
		  &(particles[0].position));
	  glDrawArrays(GL_POINTS, 0, particles.size());
	} else {
	  glBegin(GL_POINTS);
	  for(auto& p : particles) {
		//find nearest cluster

		int min_cluster = p.clusters[0];
		auto com = clusters[min_cluster].worldCom;//computeNeighborhoodCOM(clusters[min_cluster]);
		Eigen::Vector3d dir = p.position - com;
		double min_sqdist = dir.squaredNorm();
		for (auto& cInd : p.clusters) {
		  com = clusters[cInd].worldCom;//computeNeighborhoodCOM(clusters[cInd]);
		  dir = p.position - com;
		  double dist = dir.squaredNorm();
		  if (dist < min_sqdist) {
			min_sqdist = dist;
			min_cluster = cInd;
		  }
		}

		RGBColor rgb = HSLColor(2.0*acos(-1)*(min_cluster%12)/12.0, 0.7, 0.7).to_rgb();
		//RGBColor rgb = HSLColor(2.0*acos(-1)*min_cluster/clusters.size(), 0.7, 0.7).to_rgb();
		if ((which_cluster == -1 || min_cluster == which_cluster) &&
			clusters[min_cluster].members.size() > 1) {
		  //      sqrt(min_sqdist) < 0.55*clusters[min_cluster].renderWidth) {
		  glColor4d(rgb.r, rgb.g, rgb.b, 0.95);
		} else {
		  glColor4d(1,1,1,0.95);
		}
		//glPushMatrix();
		//glTranslated(p.position[0], p.position[1], p.position[2]);
		//utils::drawSphere(0.01, 4, 4);
		glVertex3dv(p.position.data());
		//glPopMatrix();
	  }
	  glEnd();
	}

	/*
	  glPointSize(3);
	  glColor3f(0,0,1);
	  glVertexPointer(3, GL_DOUBLE, sizeof(Particle),
	  &(particles[0].goalPosition));
	  glDrawArrays(GL_POINTS, 0, particles.size());
	*/
  }


  drawPlanesPretty(world.planes, 
	  world.movingPlanes, 
	  world.twistingPlanes, 
	  world.tiltingPlanes, 
	  world.elapsedTime);
  
  auto max_t = std::max_element(
	  clusters.begin(), clusters.end(),
	  [](const Cluster& a, const Cluster& b){
		return a.toughness < b.toughness;
	  })->toughness;
  
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0,1.0);
  
  //draw cluster spheres
  if(settings.drawClusters){
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glMatrixMode(GL_MODELVIEW);
	for(auto&& pr : benlib::enumerate(clusters)){
	  auto& c = pr.second;
	  const auto i = pr.first;
	  if (which_cluster == -1 || i == which_cluster) {
		glPushMatrix();
			//logic:
			//c.cg.c = original COM of cluster
			//c.worldCom = current COM in world space
			//c.restCom = current COM in rest space
			//c.worldCom - c.restCom = translation of center of mass rest to world
			//trans = location of c in world.
			//auto trans = cg.c + c.worldCom - c.restCom;
			//auto trans = c.worldCom - c.restCom;
			//glTranslated(trans(0), trans(1), trans(2));
			/*
			//step 3, translate from rest space origin to world 
			glTranslated(c.worldCom(0), c.worldCom(1), c.worldCom(2)); 

			//step 2, rotate about rest-to-world transform
			Eigen::Matrix4d gl_rot = Eigen::Matrix4d::Identity();
			//push the rotation onto a 4x4 glMatrix
			//gl_rot.block<3,3>(0,0) << c.restToWorldTransform;
			auto polar = utils::polarDecomp(c.restToWorldTransform);
			gl_rot.block<3,3>(0,0) << polar.first;
			glMultMatrixd(gl_rot.data());

			//step 1, translate rest com to origin
			glTranslated(-c.restCom(0), -c.restCom(1), -c.restCom(2));
			*/

		Eigen::Matrix4d vis_t = c.getVisTransform();
		glMultMatrixd(vis_t.data());
		
		RGBColor rgb = HSLColor(2.0*acos(-1)*(i%12)/12.0, 0.7, 0.7).to_rgb();
		//RGBColor rgb = HSLColor(2.0*acos(-1)*i/clusters.size(), 0.7, 0.7).to_rgb();
		if (settings.colorByToughness) {
		  if (c.toughness == std::numeric_limits<double>::infinity()) {
			rgb = RGBColor(0.0, 0.0, 0.0);
		  } else {
			auto factor = c.toughness/max_t;
			rgb = RGBColor(1.0-factor, factor, factor);
		  }
		}
		glPolygonMode(GL_FRONT,GL_FILL);
		glColor4d(rgb.r, rgb.g, rgb.b, 0.6);
		//utils::drawSphere(c.cg.r, 10, 10);
		utils::drawClippedSphere(c.cg.r, 30, 30, c.cg.c, c.cg.planes);
		glPolygonMode(GL_FRONT,GL_LINE);
		glColor3d(0,0,0);
		utils::drawClippedSphere(c.cg.r, 30, 30, c.cg.c, c.cg.planes);
		glPopMatrix();
	  }
	}
	glDisable(GL_CULL_FACE);
  }
  
  //draw fracture planes 
  glMatrixMode(GL_MODELVIEW);
  if(settings.drawFracturePlanes){
	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_BACK);
	for(auto&& pr : benlib::enumerate(clusters)){
	  auto& c = pr.second;
	  const auto i = pr.first;
	  if (which_cluster == -1 || i == which_cluster) {
		glPushMatrix();
		
		auto& cg = c.cg;
		
		if (cg.planes.size() > 0) {
		  //step 3, translate from rest space origin to world 
		  /*
			glTranslated(c.worldCom(0), c.worldCom(1), c.worldCom(2)); 
			
			//step 2, rotate about rest-to-world transform
			Eigen::Matrix4d gl_rot = Eigen::Matrix4d::Identity();
			//push the rotation onto a 4x4 glMatrix
			//gl_rot.block<3,3>(0,0) << c.restToWorldTransform;
			auto polar = utils::polarDecomp(c.restToWorldTransform);
			gl_rot.block<3,3>(0,0) << polar.first;
			glMultMatrixd(gl_rot.data());
			
			//step 1, translate rest com to origin
			glTranslated(-c.restCom(0), -c.restCom(1), -c.restCom(2));
		  */
		  
		  if (settings.joshDebugFlag) {
			Eigen::Matrix4d vis_t = c.getVisTransform();
			glMultMatrixd(vis_t.data());
			
			RGBColor rgb = HSLColor(2.0*acos(-1)*(i%12)/12.0, 0.7, 0.7).to_rgb();
			for (auto &p : cg.planes) {
			  glPolygonMode(GL_FRONT, GL_FILL);
			  glColor4d(rgb.r, rgb.g, rgb.b, 0.5);
			  //if rest com translated to origin (step 1 above) 
			  ////then we project from c.cg.c to make support point
			  utils::drawPlane(p.first, p.second, c.cg.r, c.cg.c);
			  glPolygonMode(GL_FRONT, GL_LINE);
			  glColor3d(0,0,0);
			  utils::drawPlane(p.first, p.second, c.cg.r, c.cg.c);
			  //if rest com translated to origin (step 1 above) 
			}
		  } else {
			Eigen::Matrix4d vis_t = c.getVisTransform();
			//glMultMatrixd(vis_t.data());
			
			RGBColor rgb = HSLColor(2.0*acos(-1)*(i%12)/12.0, 0.7, 0.7).to_rgb();
			for (auto &p : cg.planes) {
			  glPolygonMode(GL_FRONT, GL_FILL);
			  glColor4d(rgb.r, rgb.g, rgb.b, 0.5);
			  //if rest com translated to origin (step 1 above) 
			  ////then we project from c.cg.c to make support point

			  Eigen::Vector4d norm4(p.first(0), p.first(1), p.first(2), 0);
			  Eigen::Vector4d pt4(c.cg.c(0), c.cg.c(1), c.cg.c(2), 1);
			  double d = norm4.dot(pt4) + p.second;
			  pt4 = pt4 - d*norm4;
			  
			  norm4 = vis_t * norm4;
			  pt4 = vis_t * pt4;
			  
			  Eigen::Vector3d norm3(norm4(0), norm4(1), norm4(2));
			  Eigen::Vector3d pt3(pt4(0), pt4(1), pt4(2));
			  d = norm3.dot(pt3);
			  
			  
			  utils::drawPlane(norm3, -d, c.cg.r, pt3);
			}
			
			//only draw the plane outlines with the transform
			glMultMatrixd(vis_t.data());
			for (auto &p : cg.planes) {
			  glPolygonMode(GL_FRONT, GL_LINE);
			  glColor3d(0,0,0);
			  utils::drawPlane(p.first, p.second, c.cg.r, c.cg.c);
			  //if rest com translated to origin (step 1 above) 
			}
		  }
		}
		glPopMatrix();
	  }
	}
	//glDisable(GL_CULL_FACE);
  }
  
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  
  if (settings.which_cluster != -1) {
	const auto& c = clusters[which_cluster];
	
	Eigen::Matrix3d Apq = world.computeApq(c);
	Eigen::Matrix3d A = Apq*c.aInv;
	if (world.nu > 0.0) A = A*c.Fp.inverse(); // plasticity
	
	//do the SVD here so we can handle fracture stuff
	Eigen::JacobiSVD<Eigen::Matrix3d> solver(A, 
		Eigen::ComputeFullU | Eigen::ComputeFullV);
	
	Eigen::Matrix3d U = solver.matrixU(), V = solver.matrixV();
	Eigen::Vector3d sigma = solver.singularValues();
	
	std::cout << "sigma: " << sigma << std::endl;
	
	glColor4d(0,0,0, 0.9);
	
	glPointSize(5);
	
	if (!settings.drawColoredParticles) {
	  glBegin(GL_POINTS);
	  for(auto &member : c.members){
		glVertex3dv(particles[member.index].position.data());
	  }
	  glEnd();
	}
  }
  
  
  
  glColor3d(1, 0, 1);
  for(auto& projectile : world.projectiles){
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	auto currentPosition = projectile.start + world.elapsedTime*projectile.velocity;
	glTranslated(currentPosition.x(), currentPosition.y(), currentPosition.z());
	utils::drawSphere(projectile.radius, 10, 10);
	glPopMatrix();
  }
  
  glColor3d(0,1,0);
  for(auto& cylinder : world.cylinders){
	utils::drawCylinder(cylinder.supportPoint, cylinder.normal, cylinder.radius);
  }
  
  glFlush();
  SDL_GL_SwapWindow(window);
}


	
	

void drawPlanesPretty(const std::vector<Eigen::Vector4d>& planes,
	const std::vector<MovingPlane>& movingPlanes,
	const std::vector<TwistingPlane>& twistingPlanes,
	const std::vector<TiltingPlane>& tiltingPlanes,
	double elapsedTime){
  //draw planes
  glDisable(GL_CULL_FACE);
  for(auto&& pr : enumerate(planes)){
	const auto i = pr.first;
	const auto& plane = pr.second;
	RGBColor rgb = HSLColor(0.25*acos(-1)*i/planes.size()+0.0*acos(-1), 0.3, 0.7).to_rgb();
	glColor4d(rgb.r, rgb.g, rgb.b, 1.0);

	utils::drawPlane(plane.head(3), plane.w(),100);
  }
  for(auto&& pr : enumerate(movingPlanes)){
	const auto i = pr.first; 
	const auto& plane = pr.second;
	RGBColor rgb = HSLColor(0.25*acos(-1)*i/movingPlanes.size()+1.0*acos(-1), 0.3, 0.7).to_rgb();
	glColor4d(rgb.r, rgb.g, rgb.b, 1.0);
	utils::drawPlane(plane.normal, plane.offset + elapsedTime*plane.velocity,100);
  }
  for(auto&& pr : enumerate(twistingPlanes)){
	const auto i = pr.first; 
	const auto& plane = pr.second;
	RGBColor rgb = HSLColor(0.25*acos(-1)*i/twistingPlanes.size()+0.5*acos(-1), 0.3, 0.7).to_rgb();
	glColor4d(rgb.r, rgb.g, rgb.b, 1.0);
	if (elapsedTime <= plane.lifetime)
	  drawTPlane(plane.normal, plane.offset, elapsedTime*plane.angularVelocity, plane.width);
  }
  for(auto&& pr : enumerate(tiltingPlanes)){
	const auto i = pr.first; 
	const auto& plane = pr.second;
	RGBColor rgb = HSLColor(0.25*acos(-1)*i/tiltingPlanes.size()+1.5*acos(-1), 0.3, 0.7).to_rgb();
	glColor4d(rgb.r, rgb.g, rgb.b, 1.0);
	if (elapsedTime <= plane.lifetime)
	  drawTiltPlane(plane.normal, plane.tilt, plane.offset, elapsedTime*plane.angularVelocity, plane.width);
  }
}



void drawTPlane(const Eigen::Vector3d& normal, double offset, double roffset, double width){

  Eigen::Vector3d tangent1, tangent2;
	
  tangent1 = normal.cross(Eigen::Vector3d{1,0,0});
  if(tangent1.isZero(1e-3)){
	tangent1 = normal.cross(Eigen::Vector3d{0,0,1});
	if(tangent1.isZero(1e-3)){
	  tangent1 = normal.cross(Eigen::Vector3d{0,1,0});
	}
  }
  tangent1.normalize();

  tangent2 = normal.cross(tangent1);
  tangent2.normalize(); //probably not necessary
	
  Eigen::AngleAxisd t(roffset,normal);
  tangent1 = t * tangent1; 
  tangent2 = t * tangent2; 

  const double sos = normal.dot(normal);
  const Eigen::Vector3d supportPoint{normal.x()*offset/sos,
	  normal.y()*offset/sos,
	  normal.z()*offset/sos};


	
  const double size = width;
  glBegin(GL_QUADS);
  glNormal3dv(normal.data());
  glVertex3dv((supportPoint + size*(tangent1 + tangent2)).eval().data());
  glVertex3dv((supportPoint + size*(-tangent1 + tangent2)).eval().data());
  glVertex3dv((supportPoint + size*(-tangent1 - tangent2)).eval().data());
  glVertex3dv((supportPoint + size*(tangent1  - tangent2)).eval().data());
  glEnd();

}


void drawTiltPlane(
	const Eigen::Vector3d& normal, 
	const Eigen::Vector3d& tilt, 
	double offset, double roffset, double width){

  Eigen::Vector3d tangent1, tangent2;
	
  tangent1 = normal.cross(Eigen::Vector3d{1,0,0});
  if(tangent1.isZero(1e-3)){
	tangent1 = normal.cross(Eigen::Vector3d{0,0,1});
	if(tangent1.isZero(1e-3)){
	  tangent1 = normal.cross(Eigen::Vector3d{0,1,0});
	}
  }
  tangent1.normalize();

  tangent2 = normal.cross(tangent1);
  tangent2.normalize(); //probably not necessary
	
  Eigen::AngleAxisd t(roffset,tilt);
  tangent1 = t * tangent1; 
  tangent2 = t * tangent2; 

  const double sos = normal.dot(normal);
  const Eigen::Vector3d supportPoint{normal.x()*offset/sos,
	  normal.y()*offset/sos,
	  normal.z()*offset/sos};


	
  const double size = width;
  glBegin(GL_QUADS);
  glNormal3dv(normal.data());
  glVertex3dv((supportPoint + size*(tangent1 + tangent2)).eval().data());
  glVertex3dv((supportPoint + size*(-tangent1 + tangent2)).eval().data());
  glVertex3dv((supportPoint + size*(-tangent1 - tangent2)).eval().data());
  glVertex3dv((supportPoint + size*(tangent1  - tangent2)).eval().data());
  glEnd();

}

void Camera::zoom(int amount){
  if(amount < -1){
	position -= 0.1*(lookAt - position);
  } else if(amount > 1){
	position += 0.1*(lookAt - position);
  }
}

void Camera::pan(Eigen::Vector2i oldPosition, Eigen::Vector2i newPosition){

  const Eigen::Vector2d delta = 
	(newPosition - oldPosition).eval().template cast<double>();
  
  const Eigen::Vector3d forwardVector = lookAt - position;
  const Eigen::Vector3d rightVector = forwardVector.cross(up);
  const Eigen::Vector3d upVector = rightVector.cross(forwardVector);
  
  const double scale = 0.0005;
  
  position += scale*(-delta.x()*rightVector +
	  delta.y()*upVector);
  

}

void Camera::move(bool forward){
  const double scale = 0.01* (forward ? 1 : -1);
  
  const Eigen::Vector3d delta = scale*(lookAt - position);
  
  lookAt += delta;
  position += delta;
  
}




#if 0
void World::draw(SDL_Window* window) const {
  throw("don't call me");
  glEnable(GL_DEPTH_TEST);
  glFrontFace(GL_CCW);
  glEnable(GL_CULL_FACE);
  glClearColor(0.2, 0.2, 0.2, 1);
  glClear(GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  int windowWidth, windowHeight;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);
  gluPerspective(45,static_cast<double>(windowWidth)/windowHeight,
	  .5, 100);

  
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(cameraPosition.x(), cameraPosition.y(), cameraPosition.z(),
	  cameraLookAt.x(), cameraLookAt.y(), cameraLookAt.z(),
	  cameraUp.x(), cameraUp.y(), cameraUp.z());



  drawPlanes();
  
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDisable(GL_DEPTH_TEST);
  //draw clusters
  glMatrixMode(GL_MODELVIEW);
  if(drawClusters){
	for(auto&& pr : benlib::enumerate(clusters)){
	  auto& c = pr.second;
	  auto i = pr.first;
	  glPushMatrix();

	  auto com = sumWeightedWorldCOM(c.members);
	  glTranslated(com.x(), com.y(), com.z());
	  glColor4d(i/(2.0*clusters.size()), 1.0, 1.0, 0.3);
	  utils::drawSphere(c.renderWidth, 10, 10);
	  glPopMatrix();
	}
  }
  glEnable(GL_DEPTH_TEST);


  
  if(!particles.empty()){						
	//	glDisable(GL_DEPTH_TEST);
	glColor3f(1,1,1);
	glPointSize(5);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_DOUBLE,
		sizeof(Particle),
		&(particles[0].position));
	glDrawArrays(GL_POINTS, 0, particles.size());
	/*	glPointSize(3);
		glColor3f(0,0,1);
		glVertexPointer(3, GL_DOUBLE, sizeof(Particle),
		&(particles[0].goalPosition));
		glDrawArrays(GL_POINTS, 0, particles.size());
	*/

  }


  glFlush();
  SDL_GL_SwapWindow(window);
}
#endif

#if 0 //unused
void World::drawSingleCluster(SDL_Window* window, int frame) const {

glEnable(GL_DEPTH_TEST);
glFrontFace(GL_CCW);
glEnable(GL_CULL_FACE);
glClearColor(0.2, 0.2, 0.2, 1);
glClear(GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT);

glMatrixMode(GL_PROJECTION);
glLoadIdentity();

int windowWidth, windowHeight;
SDL_GetWindowSize(window, &windowWidth, &windowHeight);
gluPerspective(45,static_cast<double>(windowWidth)/windowHeight,
				 .5, 100);

  
glMatrixMode(GL_MODELVIEW);
glLoadIdentity();
gluLookAt(cameraPosition.x(), cameraPosition.y(), cameraPosition.z(),
			cameraLookAt.x(), cameraLookAt.y(), cameraLookAt.z(),
			cameraUp.x(), cameraUp.y(), cameraUp.z());



drawPlanes();

glDisable(GL_DEPTH_TEST);
//draw clusters
glMatrixMode(GL_MODELVIEW);
auto i = frame % clusters.size();
auto& c = clusters[i];

glPushMatrix();

auto com = sumWeightedWorldCOM(c.members);
glTranslated(com.x(), com.y(), com.z());
glColor4d(static_cast<double>(i)/clusters.size(), 0, 1.0, 0.3);
utils::drawSphere(c.width, 10, 10);
glPopMatrix();
glEnable(GL_DEPTH_TEST);


  
//	glDisable(GL_DEPTH_TEST);
glColor3f(1,1,1);
glPointSize(3);
  
glBegin(GL_POINTS);
for(auto &member : c.members){
glVertex3dv(particles[member.first].position.data());
}
glEnd();

glFlush();
SDL_GL_SwapWindow(window);
}
#endif
#if 0 //unused
void World::drawPlanes() const{
//draw planes
glDisable(GL_CULL_FACE);
double totalCount = planes.size() + movingPlanes.size() + twistingPlanes.size() + tiltingPlanes.size();
for(auto&& pr : enumerate(planes)){
const auto i = pr.first;
const auto& plane = pr.second;
glColor4d(0.5, static_cast<double>(i)/totalCount,
			0.5, 1);

utils::drawPlane(plane.head(3), plane.w(),100);
}
glDepthMask(false);
for(auto&& pr : enumerate(movingPlanes)){
const auto i = pr.first + planes.size();
const auto& plane = pr.second;
glColor4d(0.5, i/totalCount, 0.5, 1);
utils::drawPlane(plane.normal, plane.offset + elapsedTime*plane.velocity,100);
}
for(auto&& pr : enumerate(twistingPlanes)){
const auto i = pr.first + planes.size() + movingPlanes.size();
const auto& plane = pr.second;
glColor4d(0.5, i/totalCount, 0.5, 1);
if (elapsedTime <= plane.lifetime)
  drawTPlane(plane.normal, plane.offset, elapsedTime*plane.angularVelocity, plane.width);
}
for(auto&& pr : enumerate(tiltingPlanes)){
const auto i = pr.first + planes.size() + movingPlanes.size() + twistingPlanes.size();
const auto& plane = pr.second;
glColor4d(0.5, i/totalCount, 0.5, 1);
if (elapsedTime <= plane.lifetime)
  drawTiltPlane(plane.normal, plane.tilt, plane.offset, elapsedTime*plane.angularVelocity, plane.width);
}

glDepthMask(true);
}
#endif
