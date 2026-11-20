
undefined1 __thiscall FUN_0059e5c0(int param_1,float param_2,char param_3)

{
  int *piVar1;
  float fVar2;
  bool bVar3;
  bool bVar4;
  bool bVar5;
  bool bVar6;
  char cVar7;
  char cVar8;
  byte bVar9;
  
  FUN_00401710(0);
  *(undefined1 *)(param_1 + 0x19f) = 0;
  if (param_3 != '\0') {
    FUN_0059cd50(param_2);
  }
  if (*(int *)(param_1 + 200) == 0) {
    _param_3 = 0.0;
  }
  else {
    _param_3 = **(float **)(param_1 + 0xc4);
  }
  if (*(uint *)(param_1 + 200) < 2) {
    fVar2 = 0.0;
  }
  else {
    fVar2 = *(float *)(*(int *)(param_1 + 0xc4) + 4);
  }
  if ((*(char *)(param_1 + 0x1a2) != '\0') &&
     (((_param_3 < 1.0 || (fVar2 < 1.0)) && (*(int *)(param_1 + 0x188) != 4)))) {
    FUN_00401710(0);
    FUN_0059e0e0(0,1,1,0,0);
    FUN_005a2fd0(0);
    (**(code **)(*(int *)(param_1 + 0x28) + 0x10))(1);
    FUN_005a2fd0(0);
    (**(code **)(*(int *)(param_1 + 0x40) + 0x10))(1);
    FUN_005a2fd0(0);
    (**(code **)(*(int *)(param_1 + 0x58) + 0x10))(1);
    (**(code **)(*(int *)(param_1 + 0x88) + 0x10))(1);
    (**(code **)(*(int *)(param_1 + 0xb8) + 0x10))(1);
    (**(code **)(*(int *)(param_1 + 0xe8) + 0x10))(1);
    piVar1 = (int *)(param_1 + 0x1ac);
    *piVar1 = *piVar1 + -1;
    if (*piVar1 == 0) {
      InterlockedCompareExchange((LONG *)(param_1 + 0x1a8),0,*(LONG *)(param_1 + 0x1a8));
    }
    if ((*(int *)(param_1 + 200) != 0) && (**(float **)(param_1 + 0xc4) != 1.0)) {
      **(float **)(param_1 + 0xc4) = 1.0;
      *(undefined1 *)(param_1 + 0xcc) = 0;
    }
    if ((1 < *(uint *)(param_1 + 200)) && (*(float *)(*(int *)(param_1 + 0xc4) + 4) != 1.0)) {
      *(undefined4 *)(*(int *)(param_1 + 0xc4) + 4) = 0x3f800000;
      *(undefined1 *)(param_1 + 0xcc) = 0;
    }
    *(undefined1 *)(param_1 + 0x19f) = 1;
    *(undefined4 *)(param_1 + 0x188) = 4;
    *(undefined4 *)(param_1 + 0x18c) = 0;
  }
  if (param_2 <= 0.0) goto LAB_0059ea4d;
  cVar7 = (**(code **)(*(int *)(param_1 + 0x10) + 0x28))(param_2,*(undefined4 *)(param_1 + 0xc));
  if ((*(char *)(param_1 + 0x1a4) == '\0') && (*(char *)(param_1 + 0x1a3) != '\0')) {
    cVar8 = '\x01';
LAB_0059e80f:
    if (cVar7 == '\0') {
      if (cVar8 != '\0') {
        (**(code **)(*(int *)(param_1 + 0x88) + 0x18))(param_1 + 0x28);
        goto LAB_0059e84e;
      }
LAB_0059e852:
      bVar6 = false;
    }
    else {
LAB_0059e816:
      (**(code **)(*(int *)(param_1 + 0x88) + 0x18))(param_1 + 0x10);
      if (cVar8 != '\0') {
        (**(code **)(*(int *)(param_1 + 0x88) + 0xc))(param_1 + 0x28,0x3f800000,0,0);
      }
      if (cVar7 == '\0') {
LAB_0059e84e:
        if (cVar8 == '\0') goto LAB_0059e852;
      }
      bVar6 = true;
    }
  }
  else {
    cVar8 = (**(code **)(*(int *)(param_1 + 0x28) + 0x2c))();
    if (cVar8 != '\0') goto LAB_0059e80f;
    if (cVar7 != '\0') goto LAB_0059e816;
    if ((*(char *)(param_1 + 0x1a6) != '\0') || (*(int *)(param_1 + 0xc) == 0)) goto LAB_0059e852;
    FUN_0059d870((int *)(param_1 + 0xc),&DAT_01b1a470);
    bVar6 = false;
  }
  cVar7 = FUN_0059d210(param_2);
  if ((*(char *)(param_1 + 0x1a4) == '\0') && (*(char *)(param_1 + 0x1a3) != '\0')) {
    cVar8 = '\x01';
  }
  else {
    cVar8 = (**(code **)(*(int *)(param_1 + 0x40) + 0x2c))();
  }
  if (cVar7 == '\0') {
    if (cVar8 != '\0') {
      (**(code **)(*(int *)(param_1 + 0xb8) + 0x18))(param_1 + 0x40);
      goto LAB_0059e925;
    }
    bVar5 = false;
  }
  else {
    (**(code **)(*(int *)(param_1 + 0xb8) + 0x18))(param_1 + 0xa0);
    if (cVar8 != '\0') {
      (**(code **)(*(int *)(param_1 + 0xb8) + 0xc))(param_1 + 0x40,0x3f800000,0,0);
    }
LAB_0059e925:
    bVar5 = true;
  }
  cVar7 = FUN_0059d140(param_2);
  if ((*(char *)(param_1 + 0x1a4) == '\0') && (*(char *)(param_1 + 0x1a3) != '\0')) {
    cVar8 = '\x01';
  }
  else {
    cVar8 = (**(code **)(*(int *)(param_1 + 0x58) + 0x2c))();
  }
  if (cVar7 == '\0') {
    if (cVar8 != '\0') {
      (**(code **)(*(int *)(param_1 + 0xe8) + 0x18))(param_1 + 0x58);
      goto LAB_0059e9c4;
    }
    bVar3 = false;
  }
  else {
    (**(code **)(*(int *)(param_1 + 0xe8) + 0x18))(param_1 + 0xd0);
    if (cVar8 != '\0') {
      (**(code **)(*(int *)(param_1 + 0xe8) + 0xc))(param_1 + 0x58,0x3f800000,0,0);
    }
LAB_0059e9c4:
    bVar3 = true;
  }
  if (((*(char *)(param_1 + 0x1a4) == '\0') && (*(char *)(param_1 + 0x1a3) != '\0')) ||
     (cVar7 = (**(code **)(*(int *)(param_1 + 0x70) + 0x2c))(), cVar7 != '\0')) {
    (**(code **)(*(int *)(param_1 + 0x118) + 0x18))(param_1 + 0x70);
    bVar4 = true;
  }
  else {
    bVar4 = false;
  }
  if (((bVar6) || (bVar5)) || ((bVar3 || (bVar4)))) {
    bVar9 = 1;
  }
  else {
    bVar9 = 0;
  }
  *(byte *)(param_1 + 0x19f) = *(byte *)(param_1 + 0x19f) | bVar9;
  FUN_0059e260();
LAB_0059ea4d:
  *(undefined1 *)(param_1 + 0x1a4) = *(undefined1 *)(param_1 + 0x1a3);
  piVar1 = (int *)(param_1 + 0x1ac);
  *piVar1 = *piVar1 + -1;
  if (*piVar1 == 0) {
    InterlockedCompareExchange((LONG *)(param_1 + 0x1a8),0,*(LONG *)(param_1 + 0x1a8));
  }
  return *(undefined1 *)(param_1 + 0x19f);
}

