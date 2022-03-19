#include "Gameplay/Components/RotatingBehaviour.h"

#include "Gameplay/GameObject.h"

#include "Utils/ImGuiHelper.h"
#include "Utils/JsonGlmHelpers.h"

RotatingBehaviour::RotatingBehaviour(float startTime) :
    IComponent(),
    _segmentIndex(0),
    _TravelTime(5.0f),
    _startTime(startTime),
    _timeStored(0.f),
    keyframe(0.f),
    _speed(1.f),
    _journeyLength(0.f),

    _timer(1.0f)
{ }
RotatingBehaviour::RotatingBehaviour() :
    IComponent(),
    _segmentIndex(0),
    _TravelTime(5.0f),
    _startTime(0.f),
    _timeStored(0.f),
    keyframe(0.f),
    _speed(1.f),
    _journeyLength(0.f),
    _timer(1.0f)
{ }

void RotatingBehaviour::Awake() {
    keypoints.push_back(GetGameObject()->GetPosition().z);
    keypoints.push_back(GetGameObject()->GetPosition().z + 2);
    keypoints.push_back(GetGameObject()->GetPosition().z);

    _journeyLength = std::abs(keypoints[keyframe] - keypoints[keyframe + 1]);
}

void RotatingBehaviour::Update(float deltaTime) {
	GetGameObject()->SetRotation(GetGameObject()->GetRotationEuler() + RotationSpeed * deltaTime);

    _timer += deltaTime;

    // Distance moved equals elapsed time times speed..
    float distCovered = (_timer - _startTime - _timeStored) * _speed;

    // Fraction of journey completed equals current distance divided by total distance.
    float fractionOfJourney = distCovered / _journeyLength;

    if (keyframe == keypoints.size() - 1)
    {
        keyframe = 0;
    }

    float sqt = (fractionOfJourney) * (fractionOfJourney);

    float SlowInOut = sqt / (2.0f * (sqt - fractionOfJourney) + 1.0f);

    glm::vec3 currentPosition = GetGameObject()->GetPosition();

    GetGameObject()->SetPostion(glm::vec3(currentPosition.x, currentPosition.y, Lerp(keypoints[keyframe], keypoints[keyframe + 1], SlowInOut)));

    if ((fractionOfJourney >= 1.f) && (keyframe != keypoints.size() - 1))
    {
        _timeStored = _timer - _startTime;
        keyframe++;
    }

}

void RotatingBehaviour::RenderImGui() {
	LABEL_LEFT(ImGui::DragFloat3, "Speed", &RotationSpeed.x);
}

nlohmann::json RotatingBehaviour::ToJson() const {
	return {
		{ "speed", RotationSpeed }
	};
}

RotatingBehaviour::Sptr RotatingBehaviour::FromJson(const nlohmann::json& data) {
	RotatingBehaviour::Sptr result = std::make_shared<RotatingBehaviour>();
	result->RotationSpeed = JsonGet(data, "speed", result->RotationSpeed);
	return result;
}

// Templated LERP function returns positon at current time for LERP
template <typename T>
T RotatingBehaviour::Lerp(const T& p0, const T& p1, float t)
{
    return (1.0f - t) * p0 + t * p1;
}

