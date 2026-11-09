#pragma once

#include <QByteArray>

struct EncryptionAlgorithmParameters
{
	virtual ~EncryptionAlgorithmParameters();
};

class EncryptionAlgorithm
{
public:
	enum class Mode { Encrypt, Decrypt };

	virtual ~EncryptionAlgorithm();
	virtual QByteArray Update(const QByteArray& input) = 0;
	virtual QByteArray Finish() = 0;
};
