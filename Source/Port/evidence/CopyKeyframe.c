
void __thiscall FUN_005a3040(int *param_1,int *param_2)

{
  char cVar1;
  uint uVar2;
  float fVar3;
  
  if ((param_2 != (int *)0x0) && (cVar1 = (**(code **)(*param_2 + 0x30))(), cVar1 != '\0')) {
    *(char *)(param_1 + 5) = (char)param_2[5];
    if (param_2[1] != param_1[1]) {
      param_1[1] = param_2[1];
    }
    if ((float)param_1[2] != (float)param_2[2]) {
      param_1[2] = param_2[2];
    }
    if (param_2[4] != param_1[4]) {
      (**(code **)(*param_1 + 0x3c))(param_2[4],1);
    }
    uVar2 = 0;
    if (param_1[4] != 0) {
      do {
        if (uVar2 < (uint)param_2[4]) {
          fVar3 = *(float *)(param_2[3] + uVar2 * 4);
        }
        else {
          fVar3 = 0.0;
        }
        if ((uVar2 < (uint)param_1[4]) && (fVar3 != *(float *)(param_1[3] + uVar2 * 4))) {
          *(float *)(param_1[3] + uVar2 * 4) = fVar3;
          *(undefined1 *)(param_1 + 5) = 0;
        }
        uVar2 = uVar2 + 1;
      } while (uVar2 < (uint)param_1[4]);
    }
  }
  return;
}

