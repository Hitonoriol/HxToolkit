#include "EncryptionAlgorithmFactory.h"

#include "AesEncryptionAlgorithm.h"

#include <stdexcept>
#include <utility>

EncryptionAlgorithmFactory::EncryptionAlgorithmFactory(Algorithm value,
	std::unique_ptr<EncryptionAlgorithmParameters> configuration)
	: algorithm(value)
	, parameters(std::move(configuration))
{
	if (!parameters) {
		throw std::invalid_argument("Encryption algorithm parameters are required.");
	}
}

EncryptionAlgorithmFactory::Algorithm EncryptionAlgorithmFactory::GetAlgorithm() const
{
	return algorithm;
}

QByteArray EncryptionAlgorithmFactory::GenerateKey() const
{
	switch (algorithm) {
	case Algorithm::Aes:
		return AesEncryptionAlgorithm::GenerateKey(*parameters);
	}

	throw std::invalid_argument("The encryption parameters do not match the selected algorithm.");
}

std::unique_ptr<EncryptionAlgorithm> EncryptionAlgorithmFactory::Create(EncryptionAlgorithm::Mode mode,
	const QByteArray& key) const
{
	switch (algorithm) {
	case Algorithm::Aes:
		return std::make_unique<AesEncryptionAlgorithm>(mode, *parameters, key);
	}

	throw std::invalid_argument("The encryption parameters do not match the selected algorithm.");
}
