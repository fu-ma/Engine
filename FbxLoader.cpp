#include "FbxLoader.h"
#include <cassert>

using namespace DirectX;

/// <summary>
/// �ÓI�����o�ϐ��̎���
/// </summary>
const std::string FbxLoader::baseDirectory = "Resources/";

/// <summary>
/// �e�N�X�`�����Ȃ��ꍇ�̕W���e�N�X�`���t�@�C����
/// </summary>
const std::string FbxLoader::defaultTextureFileName = "white1*1.png";

FbxLoader *FbxLoader::GetInstance()
{
    static FbxLoader instance;
    return &instance;
}

void FbxLoader::Initialize(ID3D12Device *device)
{
    //�ď������`�F�b�N
    assert(fbxManager == nullptr);
    //�������烁���o�ϐ��ɑ��
    this->device = device;
    //FBX�}�l�[�W���̐���
    fbxManager = FbxManager::Create();
    //FBX�}�l�[�W���̓��o�͐ݒ�
    FbxIOSettings *ios = FbxIOSettings::Create(fbxManager, IOSROOT);
    fbxManager->SetIOSettings(ios);
    //FBX�C���|�[�^�̐���
    fbxImporter = FbxImporter::Create(fbxManager, "");
}

FBXModel* FbxLoader::LoadModelFromFile(const string &modelName)
{
    // ���f���Ɠ������O�̃t�H���_����ǂݍ���
    const string directoryPath = baseDirectory + modelName + "/";
    // �g���q,fbx��t��
    const string fileName = modelName + ".fbx";
    //�A�����ăt���p�X�𓾂�
    const string fullpath = directoryPath + fileName;

    //�t�@�C�������w�肵��FBX�t�@�C����ǂݍ���
    if (!fbxImporter->Initialize(fullpath.c_str(), -1, fbxManager->GetIOSettings()))
    {
        assert(0);
    }

    //�V�[������
    FbxScene *fbxScene = FbxScene::Create(fbxManager, "fbxScene");

    //�t�@�C�����烍�[�h����FBX�̏����V�[���ɃC���|�[�g
    fbxImporter->Import(fbxScene);

    //���f������
    FBXModel *model = new FBXModel();
    model->name = modelName;

    //FBX�m�[�h�̐����擾
    int nodeCount = fbxScene->GetNodeCount();
    //���炩���ߕK�v�����̃��������m�ۂ��邱�ƂŁA�A�h���X�������̂�\�h
    model->nodes.reserve(nodeCount);

    //���[�g�m�[�h���珇�ɉ�͂��ă��f���ɗ�������
    ParseNodeRecursive(model, fbxScene->GetRootNode());
    //FBX�V�[���J��
    fbxScene->Destroy();

    //�o�b�t�@����
    model->CreateBuffers(device);

    return model;
}

void FbxLoader::ParseNodeRecursive(FBXModel *model, FbxNode *fbxNode, Node *parent)
{
    //�m�[�h�����擾
    string name = fbxNode->GetName();

    //���f���Ƀm�[�h��ǉ�
    model->nodes.emplace_back();
    Node &node = model->nodes.back();

    //�m�[�h�����擾
    node.name = fbxNode->GetName();

    //FBX�m�[�h�̃��[�J���ړ����
    FbxDouble3 rotation = fbxNode->LclRotation.Get();
    FbxDouble3 scaling = fbxNode->LclScaling.Get();
    FbxDouble3 translation = fbxNode->LclTranslation.Get();

    //�`���ϊ����đ��
    node.rotation = { (float)rotation[0],(float)rotation[1],(float)rotation[2],0.0f };
    node.scaling = { (float)scaling[0],(float)scaling[1],(float)scaling[2],0.0f };
    node.translation = { (float)translation[0],(float)translation[1],(float)translation[2],1.0f };

    //��]�p��Degree�i�x�j���烉�W�A���ɕϊ�
    node.rotation.m128_f32[0] = XMConvertToRadians(node.rotation.m128_f32[0]);
    node.rotation.m128_f32[1] = XMConvertToRadians(node.rotation.m128_f32[1]);
    node.rotation.m128_f32[2] = XMConvertToRadians(node.rotation.m128_f32[2]);

    //�X�P�[���A��]�A���s�ړ��s��̌v�Z
    XMMATRIX matScaling, matRotation, matTranslation;
    matScaling = XMMatrixScalingFromVector(node.scaling);
    matRotation = XMMatrixRotationRollPitchYawFromVector(node.rotation);
    matTranslation = XMMatrixTranslationFromVector(node.translation);

    //���[�J���ό`�s��̌v�Z
    node.transform = XMMatrixIdentity();
    node.transform *= matScaling; //���[���h�s��ɃX�P�[�����O�𔽉f
    node.transform *= matRotation; //���[���h�s��ɉ�]�𔽉f
    node.transform *= matTranslation; //���[���h�s��ɕ��s�ړ��𔽉f

    //�O���[�o���ό`�s��̌v�Z
    node.globalTransform = node.transform;
    if (parent)
    {
        node.parent = parent;
        //�e�̕ό`����Z
        node.globalTransform *= parent->globalTransform;
    }

    //FBX�m�[�h�̃��b�V���������
    FbxNodeAttribute *fbxNodeAttribute = fbxNode->GetNodeAttribute();

    if (fbxNodeAttribute)
    {
        if (fbxNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            model->meshNode = &node;
            ParseMesh(model, fbxNode);
        }
    }
    //�q�m�[�h�ɑ΂��čċA�Ăяo��
    for (int i = 0; i < fbxNode->GetChildCount(); i++)
    {
        ParseNodeRecursive(model, fbxNode->GetChild(i), &node);
    }


}

void FbxLoader::ParseMesh(FBXModel *model, FbxNode *fbxNode)
{
    //�m�[�h�̃��b�V�����擾
    FbxMesh *fbxMesh = fbxNode->GetMesh();

    //���_���W�ǂݎ��
    ParseMeshVertices(model, fbxMesh);
    //�ʂ��\������f�[�^�̓ǂݎ��
    ParseMeshFaces(model, fbxMesh);
    //�}�e���A���̓ǂݎ��
    ParseMaterial(model, fbxNode);
}

void FbxLoader::Finalize()
{
    // �e��FBX�C���X�^���X�̔j��
    fbxImporter->Destroy();
    fbxManager->Destroy();
}

void FbxLoader::ParseMeshVertices(FBXModel *model, FbxMesh *fbxMesh)
{
    auto &vertices = model->vertices;

    //���_���W�f�[�^�̐�
    const int controlPointsCount = fbxMesh->GetControlPointsCount();

    //�K�v���������_�f�[�^�z����m��
    FBXModel::VertexPosNormalUv vert{};
    model->vertices.resize(controlPointsCount, vert);

    //FBX���b�V���̒��_���W�z����擾
    FbxVector4 *pCoord = fbxMesh->GetControlPoints();

    //FBX���b�V���̑S���_���W�����f�����̔z��ɃR�s�[����
    for (int i = 0; i < controlPointsCount; i++)
    {
        FBXModel::VertexPosNormalUv &vertex = vertices[i];
        //���W�̃R�s�[
        vertex.pos.x = (float)pCoord[i][0];
        vertex.pos.y = (float)pCoord[i][1];
        vertex.pos.z = (float)pCoord[i][2];
    }
}

void FbxLoader::ParseMeshFaces(FBXModel *model, FbxMesh *fbxMesh)
{
    auto &vertices = model->vertices;
    auto &indices = model->indices;

    //1�t�@�C���ɕ������b�V���̃��f���͔�Ή�
    assert(indices.size() == 0);
    //�ʂ̐�
    const int polygonCount = fbxMesh->GetPolygonCount();
    //UV�f�[�^�̐�
    const int textureUVCount = fbxMesh->GetTextureUVCount();
    //UV�����X�g
    FbxStringList uvNames;
    fbxMesh->GetUVSetNames(uvNames);

    //�ʂ��Ƃ̏��ǂݎ��
    for (int i = 0; i < polygonCount; i++)
    {
        //�ʂ��\�����钸�_�̐����擾�i3�Ȃ�O�p�`�|���S���j
        const int polygonSize = fbxMesh->GetPolygonSize(i);
        assert(polygonSize <= 4);

        //1���_������
        for (int j = 0; j < polygonSize; j++)
        {
            //FBX���_�z��̃C���f�b�N�X
            int index = fbxMesh->GetPolygonVertex(i, j);
            assert(index >= 0);

            //���_�@���ǂݍ���
            FBXModel::VertexPosNormalUv &vertex = vertices[index];
            FbxVector4 normal;
            if (fbxMesh->GetPolygonVertexNormal(i, j, normal))
            {
                vertex.normal.x = (float)normal[0];
                vertex.normal.y = (float)normal[1];
                vertex.normal.z = (float)normal[2];
            }

            //�e�N�X�`��UV�ǂݍ���
            if (textureUVCount > 0)
            {
                FbxVector2 uvs;
                bool lUnmappedUV;
                //0�Ԍ��ߑł��œǂݍ���
                if (fbxMesh->GetPolygonVertexUV(i, j, uvNames[0], uvs, lUnmappedUV))
                {
                    vertex.uv.x = (float)uvs[0];
                    vertex.uv.y = (float)uvs[1];
                }
            }

            //�C���f�b�N�X�z��ɒ��_�C���f�b�N�X�ǉ�
            //3���_�܂łȂ�
            if (j < 3)
            {
                //1�_�ǉ����A����2�_�ƎO�p�`���\�z����
                indices.push_back(index);
            }
            //4���_��
            else
            {
                //3���_�ǉ����A
                //�l�p�`��0,1,2,3�̓�2,3,0�ŎO�p�`���\�z����
                int index2 = indices[indices.size() - 1];
                int index3 = index;
                int index0 = indices[indices.size() - 3];
                indices.push_back(index2);
                indices.push_back(index3);
                indices.push_back(index0);
            }
        }
    }
}

void FbxLoader::ParseMaterial(FBXModel *model, FbxNode *fbxNode)
{
    const int materialCount = fbxNode->GetMaterialCount();
    if (materialCount > 0)
    {
        //�擪�̃}�e���A�����擾
        FbxSurfaceMaterial *material = fbxNode->GetMaterial(0);
        //�e�N�X�`����ǂݍ��񂾂��ǂ�����\���t���O
        bool textureLoaded = false;

        if (material)
        {
            //FbxSurfaceLambert�N���X���ǂ������ׂ�
            if (material->GetClassId().Is(FbxSurfaceLambert::ClassId))
            {
                FbxSurfaceLambert *lambert = static_cast<FbxSurfaceLambert *>(material);

                //�����W��
                FbxPropertyT<FbxDouble3> ambient = lambert->Ambient;
                model->ambient.x = (float)ambient.Get()[0];
                model->ambient.y = (float)ambient.Get()[1];
                model->ambient.z = (float)ambient.Get()[2];

                //�g�U���ˌ��W��
                FbxPropertyT<FbxDouble3> diffuse = lambert->Diffuse;
                model->diffuse.x = (float)diffuse.Get()[0];
                model->diffuse.y = (float)diffuse.Get()[1];
                model->diffuse.z = (float)diffuse.Get()[2];
            }

            //�f�B�t���[�Y�e�N�X�`�������o��
            const FbxProperty diffuseProperty = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
            if (diffuseProperty.IsValid())
            {
                const FbxFileTexture *texture = diffuseProperty.GetSrcObject<FbxFileTexture>();
                if (texture)
                {
                    const char *filepath = texture->GetFileName();
                    //�t�@�C���p�X����t�@�C�������o
                    string path_str(filepath);
                    string name = ExtractFileName(path_str);
                    //�e�N�X�`���ǂݍ���
                    LoadTexture(model, baseDirectory + model->name + "/" + name);
                    textureLoaded = true;
                }
            }
        }
        //�e�N�X�`�����Ȃ��ꍇ�͔��e�N�X�`���𒣂�
        if (!textureLoaded)
        {
            LoadTexture(model, baseDirectory + defaultTextureFileName);
        }
    }
}

void FbxLoader::LoadTexture(FBXModel *model, const std::string &fullpath)
{
    HRESULT result = S_FALSE;
    //WIC�e�N�X�`���̃��[�h
    TexMetadata &metadata = model->metadata;
    ScratchImage &scratchImg = model->scratchImg;
    //���j�R�[�h������ɕϊ�
    wchar_t wfilepath[128];
    MultiByteToWideChar(CP_ACP, 0, fullpath.c_str(), -1, wfilepath, _countof(wfilepath));

    result = LoadFromWICFile
    (
        wfilepath,
        WIC_FLAGS_NONE,
        &metadata,
        scratchImg
    );

    if (FAILED(result))
    {
        assert(0);
    }
}

std::string FbxLoader::ExtractFileName(const std::string &path)
{
    size_t pos1;
    //��؂蕶�� '\\' ���o�Ă����ԍŌ�̕���������
    pos1 = path.rfind('\\');
    if (pos1 != string::npos)
    {
        return path.substr(pos1 + 1, path.size() - pos1 - 1);
    }
    //��؂蕶�� '/' ���o�Ă����ԍŌ�̕���������
    pos1 = path.rfind('/');
    if (pos1 != string::npos)
    {
        return path.substr(pos1 + 1, path.size() - pos1 - 1);
    }

    return path;
}
