#pragma once
#include "IComponent.h"

/// <summary>
/// Showcases a very simple behaviour that rotates the parent gameobject at a fixed rate over time
/// </summary>
class RotatingBehaviour : public Gameplay::IComponent {
public:
	typedef std::shared_ptr<RotatingBehaviour> Sptr;

	RotatingBehaviour();

	RotatingBehaviour(float startTime);

	glm::vec3 RotationSpeed = glm::vec3(0,0,30);

	virtual void Update(float deltaTime) override;
	virtual void Awake() override;
	virtual void RenderImGui() override;

	virtual nlohmann::json ToJson() const override;
	static RotatingBehaviour::Sptr FromJson(const nlohmann::json& data);

	MAKE_TYPENAME(RotatingBehaviour);

	template <typename T>
	T Lerp(const T& p0, const T& p1, float t);

private:
	size_t _segmentIndex;
	float _timer;
	float _TravelTime;
	float _startTime;
	float _timeStored;
	float _speed;
	std::vector<float> keypoints;
	float _journeyLength;


	int keyframe;
};



