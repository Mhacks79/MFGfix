
undefined4 __thiscall FUN_005a3260(int *param_1,float param_2,int *param_3)

{
  float fVar1;
  float fVar2;
  char cVar3;
  byte bVar4;
  int *piVar5;
  uint uVar6;
  float10 fVar7;
  float10 fVar8;
  float10 extraout_ST0;
  double dVar9;
  byte bStack_55;
  
  if ((param_3 == (int *)0x0) ||
     (cVar3 = (**(code **)(*param_3 + 0x30))(), piVar5 = param_3, cVar3 == '\0')) {
    piVar5 = (int *)0x0;
  }
  if ((((param_2 <= 0.0) || (piVar5 == (int *)0x0)) || (piVar5[1] != param_1[1])) ||
     ((piVar5[4] != param_1[4] || (cVar3 = (**(code **)(*param_1 + 0x1c))(piVar5), cVar3 == '\0'))))
  {
    return 0;
  }
  fVar7 = (float10)(**(code **)(*param_3 + 0x20))();
  fVar8 = (float10)FUN_0059cb60();
  FUN_0059cb30();
  uVar6 = 0;
  fVar1 = (float)((extraout_ST0 + (float10)(double)(fVar8 * (float10)(double)fVar7)) *
                 (float10)param_2);
  if (param_1[4] != 0) {
    do {
      cVar3 = (**(code **)(*param_1 + 0x38))();
      if (cVar3 != '\0') {
        fVar7 = (float10)FUN_0059c110();
        fVar8 = (float10)FUN_0059c110();
        fVar2 = (float)((float10)(float)fVar7 - fVar8);
        dVar9 = _fabs((double)fVar2);
        if (fVar1 <= (float)dVar9) {
          if (0.0 <= fVar2) {
            if (fVar2 <= 0.0) goto LAB_005a33c3;
            fVar7 = (float10)FUN_0059c110();
            fVar7 = (float10)(float)(fVar7 + (float10)fVar1);
          }
          else {
            fVar7 = (float10)FUN_0059c110();
            fVar7 = (float10)(float)(fVar7 - (float10)fVar1);
          }
        }
        else {
          fVar7 = (float10)FUN_0059c110();
        }
        bVar4 = FUN_0059c740(uVar6,(float)fVar7);
        bStack_55 = bStack_55 | bVar4;
      }
LAB_005a33c3:
      uVar6 = uVar6 + 1;
    } while (uVar6 < (uint)param_1[4]);
  }
  if ((char)param_1[5] != '\0') {
    *(byte *)(param_1 + 5) = bStack_55;
  }
  return 1;
}

