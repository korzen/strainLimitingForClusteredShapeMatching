#include "world.h"
#include <fstream>

#include "json/json.h"
#include "utils.h"
#include "color_spaces.h"

#include "enumerate.hpp"
using benlib::enumerate;
#include "range.hpp"
using benlib::range;



void World::loadFromJson(const std::string& _filename){
  elapsedTime = 0;
  filename = _filename;

  std::ifstream ins(filename);
  
  Json::Value root;
  Json::Reader jReader;

  if(!jReader.parse(ins, root)){
	std::cout << "couldn't read input file: " << filename << '\n'
			  << jReader.getFormattedErrorMessages() << std::endl;
	exit(1);
  }


  auto particleFilesIn = root["particleFiles"];
  for(auto i : range(particleFilesIn.size())){
	readParticleFile(particleFilesIn[i].asString());
  }
  
  auto cameraPositionIn = root["cameraPosition"];
  if(!cameraPositionIn.isNull() && cameraPositionIn.isArray() && cameraPositionIn.size() == 3){
	cameraPosition.x() = cameraPositionIn[0].asDouble();
	cameraPosition.y() = cameraPositionIn[1].asDouble();
	cameraPosition.z() = cameraPositionIn[2].asDouble();
  } else {
	cameraPosition = Eigen::Vector3d{0,7, -5};
  }

  auto cameraLookAtIn = root["cameraLookAt"];
  if(!cameraLookAtIn.isNull() && cameraLookAtIn.isArray() && cameraLookAtIn.size() == 3){
	cameraLookAt.x() = cameraLookAtIn[0].asDouble();
	cameraLookAt.y() = cameraLookAtIn[1].asDouble();
	cameraLookAt.z() = cameraLookAtIn[2].asDouble();
  } else {
	cameraLookAt = Eigen::Vector3d::Zero();
  }
  
  auto cameraUpIn = root["cameraUp"];
  if(!cameraUpIn.isNull() && cameraUpIn.isArray() && cameraUpIn.size() == 3){
	cameraUp.x() = cameraUpIn[0].asDouble();
	cameraUp.y() = cameraUpIn[1].asDouble();
	cameraUp.z() = cameraUpIn[2].asDouble();
  } else {
	cameraUp = Eigen::Vector3d{0,1,0};
  }

  dt = root.get("dt",1/60.0).asDouble();
  neighborRadius = root.get("neighborRadius", 0.1).asDouble();
  nClusters = root.get("nClusters", -1).asInt();
  numConstraintIters = root.get("numConstraintIters", 5).asInt();
  alpha = root.get("alpha", 1.0).asDouble();
  omega = root.get("omega", 1.0).asDouble();
  gamma = root.get("maxStretch", 0.1).asDouble();
  gamma = root.get("gamma", gamma).asDouble();
  springDamping = root.get("springDamping", 0.0).asDouble();
  toughness = root.get("toughness", std::numeric_limits<double>::infinity()).asDouble();

  // plasticity parameters
  yield = root.get("yield", 0.0).asDouble();
  nu = root.get("nu", 0.0).asDouble();
  hardening = root.get("hardening", 0.0).asDouble();
  


  auto& gravityIn = root["gravity"];
  if(!gravityIn.isNull() && gravityIn.isArray() && gravityIn.size() == 3){
	gravity.x() = gravityIn[0].asDouble();
	gravity.y() = gravityIn[1].asDouble();
	gravity.z() = gravityIn[2].asDouble();
  } else {
	std::cout << "default gravity" << std::endl;
	gravity = Eigen::Vector3d{0, -9.81, 0};
  }

  auto& planesIn =  root["planes"];
  for(auto i : range(planesIn.size())){
	if(planesIn[i].size() != 4){
	  std::cout << "not a good plane... skipping" << std::endl;
	  continue;
	}

	//x, y, z, (of normal), then offset value
	planes.push_back(Eigen::Vector4d{planesIn[i][0].asDouble(),
		  planesIn[i][1].asDouble(),
		  planesIn[i][2].asDouble(),
		  planesIn[i][3].asDouble()});
	planes.back().head(3).normalize();
  }


  auto& movingPlanesIn = root["movingPlanes"];
  for(auto i : range(movingPlanesIn.size())){
	auto normalIn = movingPlanesIn[i]["normal"];
	if(normalIn.size() != 3){
	  std::cout << "bad moving plane, skipping" << std::endl;
	  continue;
	}
	Eigen::Vector3d normal(normalIn[0].asDouble(),
		normalIn[1].asDouble(),
		normalIn[2].asDouble());
   normal.normalize();
	movingPlanes.emplace_back(normal, 
		movingPlanesIn[i]["offset"].asDouble(),
		movingPlanesIn[i]["velocity"].asDouble());

  }
  std::cout << movingPlanes.size() << " moving planes" << std::endl;

  auto& twistingPlanesIn = root["twistingPlanes"];
  for(auto i : range(twistingPlanesIn.size())){
	auto normalIn = twistingPlanesIn[i]["normal"];
	if(normalIn.size() != 3){
	  std::cout << "bad twisting plane, skipping" << std::endl;
	  continue;
	}
	Eigen::Vector3d normal(normalIn[0].asDouble(),
		normalIn[1].asDouble(),
		normalIn[2].asDouble());
   normal.normalize();
	twistingPlanes.emplace_back(normal, 
		twistingPlanesIn[i]["offset"].asDouble(),
		twistingPlanesIn[i]["angularVelocity"].asDouble(),
		twistingPlanesIn[i]["width"].asDouble(),
		twistingPlanesIn[i].get("lifetime", std::numeric_limits<double>::max()).asDouble());

  }
  std::cout << twistingPlanes.size() << " twisting planes" << std::endl;

  auto& tiltingPlanesIn = root["tiltingPlanes"];
  for(auto i : range(tiltingPlanesIn.size())){
	auto normalIn = tiltingPlanesIn[i]["normal"];
	if(normalIn.size() != 3){
	  std::cout << "bad tilting plane, skipping" << std::endl;
	  continue;
	}
	Eigen::Vector3d normal(normalIn[0].asDouble(),
		normalIn[1].asDouble(),
		normalIn[2].asDouble());
   normal.normalize();
   auto tiltIn = tiltingPlanesIn[i]["tilt"];
	if(tiltIn.size() != 3){
	  std::cout << "bad tilting plane, skipping" << std::endl;
	  continue;
	}
	Eigen::Vector3d tilt(tiltIn[0].asDouble(),
		tiltIn[1].asDouble(),
		tiltIn[2].asDouble());
   tilt.normalize();

	tiltingPlanes.emplace_back(normal, tilt,
		tiltingPlanesIn[i]["offset"].asDouble(),
		tiltingPlanesIn[i]["angularVelocity"].asDouble(),
		tiltingPlanesIn[i]["width"].asDouble(),
		tiltingPlanesIn[i].get("lifetime", std::numeric_limits<double>::max()).asDouble());

  }
  std::cout << tiltingPlanes.size() << " tilting planes" << std::endl;

  auto& projectilesIn = root["projectiles"];
  for(auto i : range(projectilesIn.size())){
	auto& projectile = projectilesIn[i];
	Eigen::Vector3d start(projectile["start"][0].asDouble(),
		projectile["start"][1].asDouble(),
		projectile["start"][2].asDouble());
	Eigen::Vector3d vel(projectile["velocity"][0].asDouble(),
		projectile["velocity"][1].asDouble(),
		projectile["velocity"][2].asDouble());

	projectiles.emplace_back(start, vel, projectile["radius"].asDouble(),
		projectile.get("momentumScale", 0).asDouble());

  }
  
  auto& cylindersIn = root["cylinders"];
  for(auto i : range(cylindersIn.size())){
	auto& cylinder = cylindersIn[i];
	Eigen::Vector3d normal(cylinder["normal"][0].asDouble(),
		cylinder["normal"][1].asDouble(),
		cylinder["normal"][2].asDouble());

	Eigen::Vector3d supportPoint(cylinder["supportPoint"][0].asDouble(),
		cylinder["supportPoint"][1].asDouble(),
		cylinder["supportPoint"][2].asDouble());


	cylinders.emplace_back(normal, supportPoint, cylinder["radius"].asDouble());
  }


  double mass = root.get("mass", 0.1).asDouble();
  for(auto& p : particles){ p.mass = mass;}

  for(auto& p : particles){ p.outsideSomeMovingPlane = false;}

  for(auto& movingPlane : movingPlanes){
	for(auto& p : particles){
      p.outsideSomeMovingPlane |= movingPlane.outside(p);
   }
  }

  for(auto& twistingPlane : twistingPlanes){
	for(auto& p : particles){
      p.outsideSomeMovingPlane |= twistingPlane.outside(p);
   }
  }

  for(auto& tiltingPlane : tiltingPlanes){
	for(auto& p : particles){
      p.outsideSomeMovingPlane |= tiltingPlane.outside(p);
   }
  }



  
  restPositionGrid.numBuckets = 6;
  restPositionGrid.updateGrid(particles);
  
  while (!makeClusters()) {nClusters *= 1.25; neighborRadius *= 1.25;}
  std::cout<<"nClusters = "<<nClusters<<std::endl;

  //apply initial rotation/scaling, etc
  //todo, read this from json
  //Eigen::Vector3d axis{0,0,1};
  //for (auto &p : particles) {
	//p.position.x() *= 2.5;
	//p.velocity = 0.5*p.position.cross(axis);
	//auto pos = p.position;
	//.position[0] = 0.36*pos[0] + 0.48*pos[1] - 0.8*pos[2];
	//p.position[1] = -0.8*pos[0] + 0.6*pos[1];// - 0.8*pos[2];
	//p.position[2] = 0.48*pos[0] + 0.64*pos[1] + 0.6*pos[2];
  //}
}




void World::readParticleFile(const std::string& _filename){


  std::ifstream ins(_filename);
  
  Eigen::Vector3d pos;
  ins >> pos.x() >> pos.y() >> pos.z();
  //auto xpos = pos;
  //pos[0] = pos[0]/2.0;
  //pos[1] = pos[1]/2.0 + 2;
  //pos[2] = pos[2]/2.0;
  double bbMin[3] = {pos.x(), pos.y(), pos.z()}, 
	bbMax[3] = {pos.x(), pos.y(), pos.z()};
  while(ins){
	particles.emplace_back();
	particles.back().position = pos;
	particles.back().restPosition = pos;
	particles.back().velocity = Eigen::Vector3d::Zero();

	ins >> pos.x() >> pos.y() >> pos.z();
	//pos[0] = pos[0]/2.0;
	//pos[1] = pos[1]/2.0 + 2;
	//pos[2] = pos[2]/2.0;
	bbMin[0] = std::min(bbMin[0], pos.x());
	bbMin[1] = std::min(bbMin[1], pos.y());
	bbMin[2] = std::min(bbMin[2], pos.z());
	bbMax[0] = std::max(bbMax[0], pos.x());
	bbMax[1] = std::max(bbMax[1], pos.y());
	bbMax[2] = std::max(bbMax[2], pos.z());
  }

  std::cout << "total particles now: " << particles.size() << std::endl;
	std::cout << "bounding box: [" << bbMin[0] << ", " << bbMin[1] << ", "<< bbMin[2];
	std::cout << "] x [" << bbMax[0] << ", " << bbMax[1] << ", "<< bbMax[2] << "]" << std::endl;;
}

void World::saveParticleFile(const std::string& _filename) const{
  std::ofstream outs(_filename);
  
  Eigen::Vector3d pos;
	for (auto& p : particles) {
		outs << p.position.x() << " " 
			 << p.position.y() << " "
			 << p.position.z() << std::endl;
	}
	outs.close();
}

void World::printCOM() const{
  Eigen::Vector3d worldCOM = 
	std::accumulate(particles.begin(), particles.end(),
					Eigen::Vector3d{0,0,0},
					[](Eigen::Vector3d acc, const Particle& p){
					  return acc + p.mass*p.position;
					});
  double totalMass = std::accumulate(particles.begin(), particles.end(),
									 0.0, 
									 [](double acc, const Particle& p){
									   return acc + p.mass;
									 });
  
  worldCOM /= totalMass;
  std::cout << worldCOM.x() << std::endl;;
}


void World::dumpParticlePositions(const std::string& filename) const{
  std::ofstream outs(filename, std::ios_base::binary | std::ios_base::out);
  size_t numParticles = particles.size();
  outs.write(reinterpret_cast<const char*>(&numParticles), sizeof(numParticles));
  std::vector<float> positions(3*numParticles);
  for(auto i : range(particles.size())){
	positions[3*i    ] = particles[i].position.x();
	positions[3*i + 1] = particles[i].position.y();
	positions[3*i + 2] = particles[i].position.z();
  }
  outs.write(reinterpret_cast<const char*>(positions.data()), 
	  3*numParticles*sizeof(typename decltype(positions)::value_type));

}

void World::dumpColors(const std::string& filename) const {
  std::ofstream outs(filename, std::ios_base::binary | std::ios_base::out);
  size_t numParticles = particles.size();
  outs.write(reinterpret_cast<const char*>(&numParticles), sizeof(numParticles));
  std::vector<float> colors(3*numParticles);
  for(auto&& pr : benlib::enumerate(particles)) {
	//find nearest cluster
	auto i = pr.first;
	auto& p = pr.second;

	int min_cluster = p.clusters[0];
	auto com = clusters[min_cluster].worldCom;
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
	if(clusters[min_cluster].neighbors.size() > 1) {
	  //      sqrt(min_sqdist) < 0.55*clusters[min_cluster].renderWidth) {
	  //glColor4d(rgb.r, rgb.g, rgb.b, 0.8);
	  colors[3*i]  = rgb.r;
	  colors[3*i+ 1]  = rgb.g;
	  colors[3*i+ 2]  = rgb.b;
	} else {
	  colors[3*i] = 1;
	  colors[3*i + 1] = 1;
	  colors[3*i + 2] = 1;
	}
  }
  outs.write(reinterpret_cast<const char*>(colors.data()),
	  3*numParticles*sizeof(float));
}
